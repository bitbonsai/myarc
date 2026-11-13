HTML: https://myarc.dev/docs/cli-and-mcp/

[All docs](https://myarc.dev/docs/)

# CLI and MCP

myArc comes with a command-line tool, `myarc`, that drives tabs in the browser. The same tool runs an MCP server, so AI agents such as Claude Code can use the browser too. MCP, the Model Context Protocol, is the standard way AI tools discover and call outside tools.

Agents work in the Agent Space, a Space myArc keeps for them, with a bot icon. They can't see or touch tabs in your other Spaces.

## Turn it on

Browser control is off by default. Press Cmd\+, to open Settings, go to Advanced and turn on Allow agents to control myArc. The switch covers every Profile. Turning it off disconnects every tool at once.

## Install the command

The nightly carries `myarc` inside the app. Link it into your `PATH`:

```
sudo mkdir -p /usr/local/bin
sudo ln -sf "/Applications/myArc Nightly.app/Contents/Helpers/myarc" /usr/local/bin/myarc
```

The link points into the app, so it keeps working after you replace the app with a newer nightly.

## Approve each tool

The first time a tool connects, a card at the top right of your myArc window names the tool and asks whether it may control your Agent Space. Choose Allow or Don't allow. The command waits up to 60 seconds for your answer. Allow lasts until you quit myArc.

While a tool is working, the bot icon at the bottom of the sidebar shows a small blue dot. To take back control, right-click the bot and stop the tool from its menu, or click Stop next to the Agent Space's name. Stop disconnects the tool, and its next command asks you again.

## Short commands

An outline is a text version of the page, one element per line. Anything you can act on carries a reference such as `[ref=e4]`:

```
form
  textbox "Email" [ref=e1] [required] value=""
  button "Send" [ref=e4]
link "Docs" [ref=e5] -> /docs.html
```

The short commands act on the current tab and use the element references from the last outline they printed.

```
myarc open <url>             # open a tab in the Agent Space and print its outline
myarc snapshot [-i]          # print the outline; -i keeps only lines you can act on, with context
myarc click @e4              # click an element
myarc fill @e1 "text"        # replace a text field's value
myarc select @e3 "Pro"       # pick an option by value or visible text
myarc press Enter [@e1]      # send Enter, Tab, Escape, an arrow or one character
myarc back | forward | reload
myarc close                  # close the current tab
myarc tabs                   # list Agent Space tabs; * marks the current one
myarc use t12                # make another tab current
```

Every action prints a status line, what changed and the new outline, so you rarely need `snapshot` after a step. After a page navigates, references start again at `e1`, so always take them from the latest outline.

Options that work with every short command:

- `--tab t12` and `--rev 1.2` act on another tab or outline revision for one call.
- `--json` prints the raw response.
- Put `--` before text that starts with a dash, as in `myarc fill @e1 -- "-5"`.

Only http and https pages open. There is no scroll or wait command yet. `click` scrolls its target into view first, and if a page is still loading, run `snapshot` again.

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | Success |
| 1 | The command returned an error, such as `stale` for an outdated reference |
| 2 | The command line was wrong |
| 3 | myArc isn't running, or browser control is off |
| 4 | myArc refused control, for example because you chose Don't allow |

Errors go to stderr. On `stale`, run `myarc snapshot` and use the new references. On `outcome_unknown`, the action may or may not have happened, so read the page before you try again.

## Run any command

`myarc commands` lists every command that works. `myarc run` sends one of them with JSON parameters:

```
myarc run space.list
myarc run space.create '{"name":"Work"}'
myarc run space.rename '{"space_id":1,"name":"Home"}'
```

These commands work today:

- Spaces: `space.list`, `space.create`, `space.select`, `space.rename` and `space.ensure_agent`, which creates the Agent Space if it's missing.
- Tabs in the Agent Space: `tabs.list`, `tab.open`, `tab.activate`, `tab.close`, `tab.navigate`, `tab.back`, `tab.forward` and `tab.reload`.
- Pages: `page.snapshot`, `page.click`, `page.fill`, `page.select` and `page.press`.

The Space commands reach all your Spaces in that Profile, so `space.rename` can rename one of yours. The first command runs in the window you used last. Later commands stay in that window's Profile until you turn control off or quit myArc.

## Use it from an MCP client

`myarc mcp` starts the MCP server. It has one tool per command, named with an underscore in place of each dot, so `space.create` is called `space_create` there. To add it to Claude Code for every project, run:

```
claude mcp add --scope user myarc -- "/Applications/myArc Nightly.app/Contents/Helpers/myarc" mcp
```

For other MCP clients, use this server entry once `myarc` is on your `PATH`:

```
{"command": "myarc", "args": ["mcp"]}
```

## Claude Code skill

The repository has a [Claude Code skill](https://github.com/bitbonsai/myarc/tree/main/skills/myarc-browser) that teaches Claude Code to drive myArc through the short commands. Copy the `skills/myarc-browser` folder into `~/.claude/skills/`.

## How tools connect

While control is on, myArc publishes its connection details in `~/Library/Application Support/myarc/control/`. Only your user account can read that folder. Tools connect over a local socket with a token that changes each time myArc starts. Nothing listens on the network.
