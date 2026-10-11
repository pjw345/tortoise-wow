# Stock bot apostrophe regression

The SQL seed and upgrade migration are tested against disposable MySQL 8.4 and
MariaDB 11.8 services in GitHub CI. The tests import the complete new-install
seed and upgrade the original 24 affected messages. They check the reported
`Hey, how's your day going?` greeting, repeat execution, preservation of localized
columns and message flags, custom wording, unrelated text categories, and a
database with no Playerbot text table.

The repair changes only exact stock name/text pairs. It does not strip Unicode,
change locales, reset the text table or replace customized messages. The runtime
text manager keeps these strings cached; existing realms receive the repair
through the next normal database migration and text reload/startup.

Never run this test against production: it creates its own fixture databases.
Set `DATABASE_CONTAINER_ID` only to the disposable CI service. The full client
display path is checked separately in game after deployment.

The follow-up migration also checks the observed Windows-1252-decoded UTF-8 stock messages. Fixtures include original UTF-8, corrupted imports and already-normalized rows. Both migrations run twice, and custom text, localization and message metadata must remain unchanged.
