# Twitch chat

This client can display a Twitch channel in a separate in-game overlay and send
replies as your Twitch account. Game chat and Twitch chat have separate inputs.

1. Open **Settings > Twitch**.
2. Enter the channel login without `#` and your account login, both lowercase.
3. Paste a Twitch **user access token** belonging to that account, with
   `chat:read` and `chat:edit` scopes. The optional `oauth:` prefix is accepted.
   See [Twitch's authentication documentation](https://dev.twitch.tv/docs/authentication/getting-tokens-oauth/)
   for obtaining a token using your registered Twitch application. This version
   does not provide browser sign-in or automatic token refresh.
4. Click **Connect / reconnect**. The status changes to `Connected to #channel`
   after Twitch confirms the channel join.
5. Assign **Twitch chat** a key under **Settings > Controls > Chat**, or use
   `bind y twitch_chat` in the F1 console (choose an unused key).
6. While playing, press that key to type, **Enter** to queue a reply, or **Escape**
   to close the input. An unsent draft is retained. Scroll the mouse wheel while
   the input is open to read older messages.

The overlay follows the normal game chat's font size, message color, padding,
rounded message backgrounds, and old-style setting. Twitch names are purple.
Messages fade after 14 seconds and disappear after 16 seconds; opening Twitch
input restores the history for scrolling. The input is labeled `Twitch:`.
Your own messages use the display name reported by Twitch, including its casing.

The overlay shares the normal game chat's left edge, width, and scoreboard
width adjustment. **Height above game chat** in Twitch settings shifts its
messages and input upward from the normal chat position (0 aligns them; the
default is 100). Width follows the normal chat width setting. Hiding the overlay
keeps the connection open. Connecting is manual; it never connects at startup.
After a network disconnect, use **Connect / reconnect**. Leaving a game keeps the
Twitch connection open, but hides the overlay and closes the input.

The access token is masked in the UI and saved locally as plain text in the
client settings, alongside the channel, login, visibility, and height above game
chat. After restarting, the fields are restored; click **Connect / reconnect**
to connect again. **Disconnect** clears the chat history and connection while
keeping the saved credentials. Clear the access token field to forget it on the
next settings save. Expired or revoked tokens must be replaced.

The connection uses Twitch IRC over verified TLS through the client's existing
libcurl dependency. Replies are limited to 450 UTF-8 bytes, with a bounded queue
and at least 1.6 seconds between outgoing messages. Twitch notices appear in the
overlay, including messages rejected by channel restrictions. Deleted messages,
timeouts, bans, and chat clears remove matching messages from the local history.
Messages are rendered as text; emote images, badges, whispers, and moderation
controls are not implemented. Web builds do not support this connection.
