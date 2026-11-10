---
title: "Letting agents use myArc"
summary: "AI agents like Claude Code can now browse in myArc, in a Space of their own and only after you say yes. Also a Chromium crash, and the first step toward importing from Arc."
date: 2026-11-10
draft: true
---

myArc is built by agents. I lead a group of them that write and review the code, and this round they built something for agents that aren't on the team. An AI agent like Claude Code can now open pages in myArc, read them and click through them.

## How an agent reads a page

The agent drives myArc through `myarcctl`, a small command-line tool. The same program also runs as an MCP server, the common way AI apps plug in outside tools, so assistants that support MCP get the same commands.

The usual way to let an AI use a computer is screenshots. The model gets a picture of the screen, guesses where the button is and clicks at those coordinates. That works, but each step means sending a whole image, and the click lands wherever the guess lands.

myArc gives the agent a short text outline of the page instead, one heading, link, field or button per line. Anything it can act on gets a number. To sign up on a form, it fills `e1` with an email address and clicks `e2`, and myArc acts on those exact elements. Every action answers with a fresh outline, so it sees what happened without asking for another look.

![A terminal running three myarcctl commands. open prints a Sign up page outline with an Email textbox marked ref e1 and a Send button marked ref e2. fill e1 prints the same outline with the email filled in. click e2 reports that the tab navigated and prints a Thanks heading.][session]

*A sample session on a test page, drawn in the tool's real output format.*

A few lines of text weigh far less than a screenshot, and pointing at an element beats pointing at a pixel. Each number stays tied to one element. If that element is gone, or the agent is working from an old outline, myArc refuses and the agent reads the page again, so it never clicks a stand-in. Page text can't fake a number either. Whatever a page says comes back in quotes, so a sneaky line of text can't pass itself off as a button.

The agent works only in its own Agent Space, which shows up as a bot icon in the sidebar footer. Pages it opens load in the background while you keep browsing, and your other Spaces and Profiles stay out of its reach. To watch it work, click the bot. For Claude Code there's also a skill that teaches it the commands and tells it to check with you before it sends, buys or posts anything.

## Staying in control

All of this stays off until you turn it on in Settings.

![The Advanced tab of myArc's settings, with a Browser control section and an "Allow agents to control myArc" switch turned off][setting]

*While the switch is off, nothing can connect.*

The first time a tool connects, a card in your myArc window names it and asks whether it may control your Agent Space, with Allow and Don't allow. Allow holds until you quit myArc.

I want to be honest about what that card proves. Each tool names itself when it connects, so another program running on your account could claim to be `myarcctl`. The card records your consent and shows who says they're acting, but it doesn't check anyone's ID.

While an agent is working, a small blue dot sits on the bot icon and a Stop button appears next to the Agent Space's name. Stop cuts the agent off right away, and the next time it tries to connect, myArc asks you again.

![The top of the Agent Space in the sidebar, with a Stop button beside the Agent name and two Example Domain tabs below][stop]

*Stop stays next to the Agent Space's name while a tool is connected.*

![The sidebar footer with three Space icons, where the bot icon on the right carries a small blue dot][dot]

*The blue dot on the bot means an agent is at work.*

## A crash that wasn't ours

While the agents were dogfooding all this, a click in a background agent tab would now and then kill the page. It seemed to need the agent to sit idle for a bit before clicking, and for a while it looked like no crash report was being written at all. Then it turned out Chromium had been saving them the whole time, all pointing at the same place in the code.

The bug was in Chromium itself. Before a click, the browser asks where the element sits so it can scroll it into view, and that lookup read the page's layout without bringing it up to date. A tab you're looking at refreshes layout every frame, so nobody notices. A background tab draws no frames, so its layout goes stale, and one of Chromium's own safety checks caught the stale read and stopped the page. The fix is a single line that updates the layout before reading it, which the function right next to it already did. After that, the same test clicked through every round without a crash.

## Reading an Arc export

Arc can export your Spaces as a file, under Settings > Advanced > Export Data. myArc now has a reader that turns that file into a list of Spaces, the groups inside each one and their links, in Arc's order. It doesn't open any of the links while reading. A line it doesn't understand gets flagged with its line number and a reason instead of quietly disappearing, since an import that loses tabs on the way in isn't much of an import.

It doesn't create anything in the browser yet, and there's no button for it.

## Where it's going

Along the way the sidebar footer got a redesign. Your Spaces sit there as icons, and those from your other Profiles share a soft pill, so you can tell at a glance which Profile they belong to. The bot moved in right next to them.

Next up is turning an Arc import into real Spaces and pins, letting pins fetch their own icons, and more work on making myArc look like Arc. I've been using Arc since 2023, so there are a lot of Spaces waiting on that import.

[session]: agent-session.png
[setting]: browser-control-setting.png
[stop]: agent-space-stop.png
[dot]: agent-space-dot.png
