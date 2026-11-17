# Blocker fixture

A synthetic page for the bundled blocker check ([tools/check_blocker.sh](../../../tools/check_blocker.sh)). `index.html` loads `/cogi-etracker.js` from its own server. That path matches EasyPrivacy rule `/cogi-etracker.js`, which ships enabled in the bundled uBlock Origin Lite rulesets. The page title and heading read `tracker: blocked` when the request fails, `tracker: loaded` when the script ran, and `tracker: replaced` if a stand-in body loaded instead. The server log tells whether the request left the browser.

Both files are original test code. They contain no real tracker.
