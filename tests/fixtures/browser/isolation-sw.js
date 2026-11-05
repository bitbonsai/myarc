// Synthetic service worker for the profile-isolation check. It only activates.
self.addEventListener('install', () => self.skipWaiting())
self.addEventListener('activate', event => event.waitUntil(self.clients.claim()))
