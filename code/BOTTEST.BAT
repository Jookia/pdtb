@echo off
echo Running wmake...
wmake
IF ERRORLEVEL 1 GOTO :end
set OLDHOST=%BOT_HOST%
set OLDLOG=%BOT_LOG%
set OLDAUTH=%BOT_AUTH%
set BOT_HOST=10.0.2.2:6666
set BOT_LOG=BOTTEST.LOG
set BOT_AUTH=BOTAUTH.TXT
echo Running bot...
%DEBUG% bot
set BOT_HOST=%OLDHOST%
set BOT_LOG=%OLDLOG%
set BOT_AUTH=%OLDAUTH%
set OLDHOST=
SET OLDLOG=
:end
