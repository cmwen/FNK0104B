import json
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import server
from dispatcher import RepositoryCatalog
from test_server import FakeAppServer


class Router(FakeAppServer):
    def __init__(self, result, delay=None):
        super().__init__([
            {'data': [{'model': 'gpt-6-luna', 'supportedReasoningEfforts': [{'reasoningEffort': 'low'}]}]},
            {'thread': {'id': 'router'}}, {'turn': {'id': 'routing'}}])
        self.result, self.delay = result, delay

    def wait_result(self, *args):
        if self.delay:
            self.delay.wait(3)
        return json.dumps(self.result)


class CatalogTests(unittest.TestCase):
    def test_nested_worktrees_and_dependency_exclusion(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in ('board', 'group/worktree', 'node_modules/hidden'):
                repo = root / name
                repo.mkdir(parents=True)
                (repo / '.git').write_text('gitdir: elsewhere')
            external = root / 'alias'
            external.symlink_to(root / 'board', target_is_directory=True)
            catalog = RepositoryCatalog(root)
            self.assertEqual(set(catalog.scan()), {'board', 'group/worktree'})

    def test_replaced_repository_cannot_escape_root(self):
        with tempfile.TemporaryDirectory() as tmp, tempfile.TemporaryDirectory() as outside:
            path = Path(tmp) / 'repo'
            path.mkdir(); (path / '.git').mkdir()
            catalog = RepositoryCatalog(tmp)
            entry = catalog.scan()['repo']
            (path / '.git').rmdir(); path.rmdir()
            (Path(outside) / '.git').mkdir()
            path.symlink_to(outside, target_is_directory=True)
            with self.assertRaises(ValueError):
                catalog.resolve(entry)


class DispatcherTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.repo = Path(self.directory.name) / 'board'
        self.repo.mkdir(); (self.repo / '.git').mkdir()

    def tearDown(self):
        self.directory.cleanup()

    def bridge(self, route, responses=None, delay=None):
        bridge = server.Bridge(FakeAppServer(responses or []))
        bridge.enable_dispatcher(self.directory.name, router=Router(route, delay), worker_model='gpt-6.1-sol')
        return bridge

    def wait_job(self, bridge, job_id, stage):
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            job = next(j for j in bridge.dispatcher.snapshot() if j['id'] == job_id)
            if job['stage'] == stage:
                return job
            time.sleep(.01)
        self.fail(f'Expected {stage}, got {job}')

    def test_async_dispatch_preserves_original_request_and_is_idempotent(self):
        gate = threading.Event()
        bridge = self.bridge({'intent': 'task', 'repository_id': 'board', 'clarification': ''},
                             [{'thread': {'id': 'worker'}}, {'turn': {'id': 'turn'}}], gate)
        result = bridge.submit(text='Please inspect the board; do not change files.', request_id='one')
        self.assertEqual(result['action'], 'queued')
        self.assertEqual(bridge.submit(text='Please inspect the board; do not change files.', request_id='one')['job_id'], result['job_id'])
        with self.assertRaises(ValueError):
            bridge.submit(text='different', request_id='one')
        self.assertEqual(bridge.app.calls, [])
        gate.set()
        job = self.wait_job(bridge, result['job_id'], 'running')
        self.assertEqual(job['worker_id'], 'worker')
        self.assertEqual(bridge.app.calls[0], ('thread/start', {'cwd': str(self.repo), 'model': 'gpt-6.1-sol'}))
        self.assertEqual(bridge.app.calls[1][1]['input'][0]['text'], 'Please inspect the board; do not change files.')

    def test_ambiguous_command_never_starts_worker(self):
        bridge = self.bridge({'intent': 'clarify', 'repository_id': 'UNKNOWN', 'clarification': 'Which repository?'})
        result = bridge.submit(text='Fix the bug')
        self.wait_job(bridge, result['job_id'], 'clarify')
        self.assertEqual(bridge.app.calls, [])
        self.assertEqual(bridge.dispatcher.agents()[0]['status'], 'needs_attention')

    def test_invalid_selection_fails_without_worker(self):
        bridge = self.bridge({'intent': 'task', 'repository_id': '../../outside', 'clarification': ''})
        result = bridge.submit(text='Fix board')
        self.wait_job(bridge, result['job_id'], 'error')
        self.assertEqual(bridge.app.calls, [])

    def test_clarification_answer_keeps_the_original_command(self):
        bridge = self.bridge({'intent': 'clarify', 'repository_id': 'UNKNOWN', 'clarification': 'Which repository?'},
                             [{'thread': {'id': 'worker'}}, {'turn': {'id': 'turn'}}])
        question = bridge.submit(text='Report the version without edits')
        self.wait_job(bridge, question['job_id'], 'clarify')
        bridge.dispatcher.router = Router({'intent': 'task', 'repository_id': 'board', 'clarification': ''})
        answer = bridge.submit(text='The board repository', agent_id=question['job_id'])
        self.wait_job(bridge, answer['job_id'], 'running')
        prompt = bridge.app.calls[-1][1]['input'][0]['text']
        self.assertIn('Report the version without edits', prompt)
        self.assertIn('The board repository', prompt)

    def test_worker_completion_reaches_dispatcher_avatar(self):
        bridge = self.bridge({'intent': 'task', 'repository_id': 'board', 'clarification': ''},
                             [{'thread': {'id': 'worker'}}, {'turn': {'id': 'turn'}}])
        result = bridge.submit(text='Report version')
        self.wait_job(bridge, result['job_id'], 'running')
        bridge.app._dispatch({'method': 'item/completed', 'params': {'threadId': 'worker',
                              'item': {'type': 'agentMessage', 'text': 'Version 0.5.0. ' + 'x' * 200}}})
        bridge.app._dispatch({'method': 'turn/completed', 'params': {'threadId': 'worker',
                              'turn': {'id': 'turn', 'status': 'completed'}}})
        job = self.wait_job(bridge, result['job_id'], 'completed')
        self.assertTrue(job['detail'].startswith('Version 0.5.0.'))
        self.assertTrue(bridge.dispatcher.agents()[0]['name'].startswith('Done:'))

    def test_targeted_command_bypasses_router_and_preserves_approval_block(self):
        bridge = self.bridge({})
        bridge.app.pending_approvals['worker'] = {'request_id': 9}
        result = bridge.submit(text='Continue', agent_id='worker')
        job = self.wait_job(bridge, result['job_id'], 'error')
        self.assertIn('will not approve', job['detail'])
        self.assertEqual(bridge.dispatcher.router.calls, [])

    def test_voice_is_transcribed_in_background(self):
        bridge = self.bridge({'intent': 'clarify', 'repository_id': 'UNKNOWN', 'clarification': 'Which repository?'})
        bridge.transcriber = 'test'
        bridge.transcribe = lambda wav: 'Fix the bug'
        result = bridge.submit(wav=b'RIFF....WAVE')
        job = self.wait_job(bridge, result['job_id'], 'clarify')
        self.assertEqual(job['text'], 'Fix the bug')

    def test_missing_transcription_rejected_before_queueing(self):
        bridge = self.bridge({})
        with self.assertRaises(server.BridgeError):
            bridge.submit(wav=b'RIFF....WAVE')
        self.assertEqual(bridge.dispatcher.snapshot(), [])

    def test_result_collector_does_not_truncate_json_to_display_length(self):
        app = server.AppServer()
        app.connection_lost.clear()
        app.track_result('router')
        output = json.dumps({'clarification': 'x' * 500})
        app._dispatch({'method': 'item/completed', 'params': {'threadId': 'router',
                       'item': {'type': 'agentMessage', 'text': output}}})
        app._dispatch({'method': 'turn/completed', 'params': {'threadId': 'router',
                       'turn': {'id': 'turn', 'status': 'completed'}}})
        self.assertEqual(app.wait_result('router', 'turn', 1, threading.Event()), output)
        self.assertEqual(len(app.latest_messages['router']), 120)


if __name__ == '__main__':
    unittest.main()
