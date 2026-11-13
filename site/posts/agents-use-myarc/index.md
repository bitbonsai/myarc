---
title: "Letting agents use myArc"
summary: "The agents that build myArc can now use it, in a Space of their own and only after you say yes. Getting there turned up a bug in Chromium and a sidebar full of gray globes."
date: 2026-11-10
draft: false
---

myArc is built by agents. I lead a group of them that write the code, review it and test it. They could launch myArc and take screenshots of it, but they couldn't click anything in it. When they needed to browse, they used [agent-browser](https://github.com/vercel-labs/agent-browser), Vercel's browser tool for agents. I work with it and I like it, but it drives its own copy of Chrome, empty and off to the side. They were building a browser they couldn't use.

I wanted the same thing inside myArc, so this round I asked them to build it. An agent like Claude Code can now open pages in myArc, read them and click through them, in a Space of its own.

## Reading a page without looking at it

Many AI tools that use a computer work from screenshots. The model gets a picture of the screen, guesses where the button is and clicks at those coordinates. It works, but every step sends a whole image, and if the guess is a few pixels off, the click hits something else.

myArc borrows agent-browser's approach and hands the agent a short text outline of the page instead, with one heading, link, field or button per line. Everything the agent can act on gets a number. To sign up on a form, the agent fills `e1` with an email address and clicks `e2`. After each action it gets a fresh outline back, so it sees what changed without asking.

![A terminal running three myarc commands. open prints a Sign up page outline with an Email textbox marked ref e1 and a Send button marked ref e2. fill e1 prints the same outline with the email filled in. click e2 reports that the tab navigated and prints a Thanks heading.][session]

*A sample session on a test page, drawn in the tool's real output format.*

In the agents' test runs an outline came to a couple hundred characters per step. Each number stays tied to one element, so if the page changed under the agent, myArc refuses and the agent reads it again rather than clicking whatever now sits in that spot. Page text can't pass itself off as a button either, because whatever a page says comes back in quotes.

The agent gets in through `myarc`, a small command-line tool that also runs as an MCP server, the common way AI apps plug in outside tools. It works in its own Agent Space, the bot icon in the sidebar footer, and its pages load in the background while you keep browsing. Your other Spaces and Profiles stay out of its reach.

## The click that crashed

The agents started testing right away, and soon one of their clicks crashed the page. It only happened sometimes, after the agent had sat idle for a bit. For a while it looked like nothing was writing a crash report, until it turned out Chromium had saved every one, all pointing at the same spot in its code.

The bug was Chromium's own. Before a click, the browser checks where the element sits so it can scroll it into view, and that check read the page layout without updating it. A tab you're looking at redraws every frame, which keeps its layout fresh. The agent's tab sits in the background and doesn't draw, so its layout went stale, and a safety check stopped the page. The fix is one line that updates the layout first, as the function next to it already did. After that, the same test clicked through every round.

## Who gets to drive

With clicks working, the next question was how much an agent should be allowed to do without asking you. My answer is nothing, until you turn it on in Settings.

![The Advanced tab of myArc's settings, with a Browser control section and an "Allow agents to control myArc" switch turned off][setting]

*The switch starts off, and nothing can connect until you flip it.*

The first time a tool connects, a card names it and asks whether it may control your Agent Space. Allow lasts until you quit myArc.

![A myArc window with six Favorite tiles in the sidebar and a card at the top right reading "myarc 2026.11.13 wants to control your Agent Space", with Don't allow and Allow buttons][prompt]

*The card says what the tool can do and that it can't see your other Spaces.*

The card can't prove who is asking, though. Each tool names itself when it connects, so another program on your account could claim to be `myarc`. You get to say yes or no and see the name it gave, and that's all.

While an agent works, a blue dot sits on the bot icon and a Stop button appears next to the Agent Space's name. Stop cuts it off at once, and the next time it connects, myArc asks again. Claude Code also gets a skill that teaches it the commands and tells it to check with you before it sends, buys or posts anything.

![The top of the Agent Space in the sidebar, with a Stop button beside the Agent name and two Example Domain tabs below][stop]

*The Agent Space while a tool is working.*

## A sidebar full of globes

When the agents opened their test windows, I noticed every pin was a gray globe. myArc only learned a site's icon when you visited it, and those test profiles had never visited anything. After an import from Arc, everyone would have seen the same thing.

Now a pin without an icon asks its own site for one in the background and keeps it. The request sends no cookies, asks only for the site's home page, never the page you pinned, and goes straight to that site with no Google in between.

![Six Favorite tiles in the sidebar showing the GitHub, Wikipedia, MDN, Hacker News, Rust and Python icons][icons]

*None of these sites was ever opened in this profile.*

## Leaving Arc

That import is the part I've been waiting for. I've used Arc since 2023, and I'd rather not rebuild those Spaces by hand.

Arc can export your Spaces to a file, under `Settings > Advanced > Export Data`. In myArc, `Settings > Advanced > Import from Arc…` reads that file and shows what it will add before it writes anything, along with the line number of anything it couldn't read. Each Space comes over with its links as pinned tabs, in Arc's order. Import the same file twice and the second time adds nothing.

![A dialog titled Import from Arc that reads "Adds 4 Spaces and 19 pinned tabs", with a note that Favorites and folders become pinned tabs and nothing opens until you click it, and Cancel and Import buttons][import]

*The preview from a test export. Cancel leaves your Spaces as they were.*

Some of Arc doesn't make it across yet. The export doesn't say which links are Favorites, so they land as pinned tabs in each Space. Folders get flattened too, because myArc has no folders of its own.

The agents that build myArc don't need a separate Chrome anymore. They browse in a Space next to mine, inside the browser they're building. All of this is in the nightly, if you'd like to give your own agent a Space too.

[session]: agent-session.png
[setting]: browser-control-setting.png
[stop]: agent-space-stop.png
[prompt]: connect-prompt.png
[icons]: pin-icons.png
[import]: arc-import-preview.png
