#!/usr/bin/env python3
"""Reuse a complete successful main firmware run without invoking PlatformIO."""
import argparse
import json
from pathlib import Path
import subprocess
from urllib.error import HTTPError
from urllib.request import urlopen
import os

from firmware_ci import image_names, verify
from package_web_firmware import firmware_environments

WORKFLOWS = {'.github/workflows/firmware.yml', '.github/workflows/build.yml'}



class CatalogUnavailable(RuntimeError):
    """No retained or published catalog exists yet."""


def active_firmware_run(repository):
    runs = api(repository, 'actions/runs?branch=main&per_page=100')['workflow_runs']
    return next((run for run in runs if run.get('head_branch') == 'main'
                 and run.get('event') in {'push', 'workflow_dispatch'}
                 and run.get('path', '').split('@')[0] in WORKFLOWS
                 and run.get('status') in {'queued', 'in_progress', 'waiting', 'pending'}), None)


def prepare(repository, output, published_base=None, defer_if_building=False):
    try:
        fetch(repository, output, published_base)
    except CatalogUnavailable:
        pending = active_firmware_run(repository) if defer_if_building else None
        if not pending:
            raise
        message = (f"Publication deferred: no firmware catalog exists yet. "
                   f"Waiting for {pending['html_url']}; its successful completion "
                   "automatically starts Publish board guide. Existing Pages is unchanged.")
        print(message)
        if os.environ.get('GITHUB_STEP_SUMMARY'):
            with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as summary:
                summary.write(message + '\n')
        return False
    return True


def api(repository, endpoint):
    result = subprocess.run(['gh', 'api', f'repos/{repository}/{endpoint}'],
                            check=True, capture_output=True, text=True)
    return json.loads(result.stdout)


def select_artifacts(run, artifacts, environments):
    """Reject failed, non-main, unrelated, partial and expired sources."""
    if (run.get('head_branch') != 'main' or run.get('conclusion') != 'success'
            or run.get('event') not in {'push', 'workflow_dispatch'}
            or run.get('path', '').split('@')[0] not in WORKFLOWS):
        return None
    available = {artifact['name'] for artifact in artifacts if not artifact.get('expired', True)}
    if 'firmware-catalog' in available:
        return ['firmware-catalog']
    legacy = [f'firmware-{environment}' for environment in environments]
    return legacy if set(legacy).issubset(available) else None


def download_published(base, output, environments):
    """The deployed catalog is a durable fallback; metadata retains image hashes."""
    if not base.startswith('https://'):
        raise ValueError('Published firmware requires HTTPS')
    for environment in environments:
        target = output / environment
        target.mkdir(parents=True, exist_ok=True)
        prefix = f"{base.rstrip('/')}/firmware/{environment}/"
        try:
            with urlopen(prefix + 'metadata.json', timeout=30) as response:
                metadata = json.load(response)
        except HTTPError as error:
            if error.code == 404 and environment == environments[0]:
                raise CatalogUnavailable('Published catalog metadata is not available yet') from error
            raise
        if set(metadata.get('images', {})) != set(image_names(environment)):
            raise ValueError('Published firmware image list mismatch')
        (target / 'metadata.json').write_text(json.dumps(metadata) + '\n')
        for name in metadata['images']:
            destination = target / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            with urlopen(prefix + name, timeout=60) as response:
                destination.write_bytes(response.read())
        verify(output, environment)
    print(f'Reusing verified published firmware from {base}')


def fetch(repository, output, published_base=None):
    if output.exists() and any(output.iterdir()):
        raise ValueError(f'Download destination must be empty: {output}')
    environments = firmware_environments()
    # Pagination also handles repositories with many intervening docs deployments.
    for page in range(1, 11):
        runs = api(repository, f'actions/runs?branch=main&status=success&per_page=100&page={page}')['workflow_runs']
        if not runs:
            break
        for run in runs:
            if run.get('path', '').split('@')[0] not in WORKFLOWS:
                continue
            artifacts = api(repository, f"actions/runs/{run['id']}/artifacts?per_page=100")['artifacts']
            names = select_artifacts(run, artifacts, environments)
            if not names:
                continue
            output.mkdir(parents=True, exist_ok=True)
            for name in names:
                subprocess.run(['gh', 'run', 'download', str(run['id']), '--repo', repository,
                                '--name', name, '--dir', str(output)], check=True)
            # A corrupt source must fail explicitly rather than publish incomplete images.
            for environment in environments:
                verify(output, environment)
            print(f"Reusing verified firmware from {run['html_url']} ({run['head_sha']})")
            return run['id']
    if published_base:
        try:
            download_published(published_base, output, environments)
        except CatalogUnavailable:
            raise
        except (OSError, ValueError) as error:
            raise RuntimeError('Published firmware metadata/images are missing or invalid. '
                               'Run Build firmware on main once, then retry Publish board guide.') from error
        return None
    raise CatalogUnavailable('No complete, unexpired successful main firmware catalog found. '
                       'Run Build firmware on main once, then retry Publish board guide. '
                       'Documentation publishing never builds firmware automatically.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--published-base', help='Trusted deployed guide URL, used after artifact expiry')
    parser.add_argument('--defer-if-building', action='store_true', help='Skip publication only when the first catalog is still building')
    args = parser.parse_args()
    ready = prepare(args.repository, args.output, args.published_base, args.defer_if_building)
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'], 'a') as outputs:
            outputs.write(f'ready={str(ready).lower()}\n')


if __name__ == '__main__':
    main()
