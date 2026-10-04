---
title: "Display & touch"
summary: "Build a dashboard, calculator, control panel or number pad."
status: Implemented
---

## What you have
A 2.8-inch IPS display with 240 × 320 pixels and capacitive touch. Our apps commonly rotate it to a 320 × 240 landscape canvas. The display is in the ILI9341 family; touch is in the FT6336 family. The exact fitted touch suffix is **UNKNOWN**.

## How to think about a user interface
Start with the decisions a person needs to make: what is happening, what can they tap, and what confirms the action? A small screen needs a few large controls, short labels and clear states. Ask an agent for a screen map before implementation: idle, working, disconnected, listening and error.

The calculator and connectivity tool use LVGL. The monitor uses shared drawing routines and a fixed landscape layout. The recorder uses a firmware drawing routine with changed-row transfers. LVGL offers widgets and layouts; custom drawing gives precise control but needs deliberate hit targets, state management and redraw rules. A web page cannot be installed directly as the board's UI.

## What the monitor taught us
Draw only when something changes. Keep touch processing independent of expensive audio or rendering work. Hidden controls must not accept taps through a listening panel. The monitor explicitly separates local-command listening from recording a message and uses text as well as color to explain each state.

## Start here
Try `display`, then `touch`, then `calculator` or `avatar-diag`. A correct build does not establish physical colors or touch accuracy; this repository records a physical color test and an LCD readback test.

## Brief your coding agent
> Build a landscape dashboard with three large touch targets. Show idle, active and disconnected states. Reuse the shared board layer and verified color orientation. Make a host preview of every state and measure touch response before adding audio.
