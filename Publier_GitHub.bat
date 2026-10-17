@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title Publication de Bata-Z sur GitHub
set "UE=C:\Program Files\Epic Games\UE_5.5"
set "LOGS=%~dp0Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"
echo ============================================================
echo  Publication du jeu sur GitHub (depot PRIVE "Bata-Z")
echo  Ferme le jeu et Unreal avant de continuer.
echo  A relancer plus tard pour envoyer les mises a jour.
echo ============================================================
pause

rem ---------- 1. Git + GitHub CLI ----------
echo [1/5] Verification de Git et GitHub CLI...
where git >nul 2>&1
if not errorlevel 1 goto have_git
echo Installation de Git...
winget install --id Git.Git -e --silent --accept-package-agreements --accept-source-agreements
set "PATH=%PATH%;C:\Program Files\Git\cmd"
where git >nul 2>&1
if errorlevel 1 goto no_git
:have_git
where gh >nul 2>&1
if not errorlevel 1 goto have_gh
echo Installation de GitHub CLI...
winget install --id GitHub.cli -e --silent --accept-package-agreements --accept-source-agreements
set "PATH=%PATH%;C:\Program Files\GitHub CLI"
where gh >nul 2>&1
if errorlevel 1 goto no_gh
:have_gh
git lfs version >nul 2>&1
if errorlevel 1 goto no_lfs

rem ---------- 2. Assets reellement utilises ----------
echo [2/5] Analyse des assets utilises par le jeu (1 a 3 min)...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\prepare_github.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_github_full.log"
if not exist ".gitignore" goto no_ignore
type "%LOGS%\claude_github.txt"

rem ---------- 3. Depot local ----------
if exist ".git" goto have_repo
git init -b main
:have_repo
git lfs install --local
git config user.name "Henri Szabo"
git config user.email "henri.szabo.szabo@gmail.com"
git config core.autocrlf false
git config core.longpaths true
git config http.postBuffer 524288000
git config lfs.concurrenttransfers 4
echo [3/5] Ajout des fichiers (plusieurs minutes a cause des gros fichiers)...
git add -A
git diff --cached --quiet
if not errorlevel 1 goto no_changes
git commit -q -m "Bata-Z : mise a jour du jeu (code, config, assets utilises)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" --trailer "Claude-Session: https://claude.ai/code/session_011BfursW567sQJA8wLpM3ZH"
if errorlevel 1 goto commit_fail
:no_changes

rem ---------- 4. Connexion GitHub + depot prive ----------
gh auth status >nul 2>&1
if not errorlevel 1 goto authed
echo [4/5] Connexion a GitHub : appuie sur Entree, une page web s'ouvre, colle le code affiche et valide.
gh auth login --hostname github.com --git-protocol https --web
gh auth status >nul 2>&1
if errorlevel 1 goto no_auth
:authed
gh auth setup-git
git remote get-url origin >nul 2>&1
if not errorlevel 1 goto have_remote
echo Creation du depot prive Bata-Z...
gh repo create Bata-Z --private --source=. --remote=origin --description "FPS bodycam zombies - Unreal Engine 5.5"
if errorlevel 1 goto no_repo
:have_remote

rem ---------- 5. Envoi ----------
echo [5/5] Envoi sur GitHub (la premiere fois : plusieurs Go, ca peut etre long)...
git push -u origin main
if errorlevel 1 goto push_fail
for /f "delims=" %%u in ('gh repo view --json url -q .url') do set "REPO_URL=%%u"
echo.
echo  TERMINE : %REPO_URL%
echo  Dis "fini" a Claude.
pause
exit /b 0

:no_git
echo ERREUR : Git n'a pas pu etre installe. Installe-le depuis https://git-scm.com puis relance.
goto end_fail
:no_gh
echo ERREUR : GitHub CLI n'a pas pu etre installe. Installe-le depuis https://cli.github.com puis relance.
goto end_fail
:no_lfs
echo ERREUR : Git LFS manquant. Reinstalle Git depuis https://git-scm.com (option Git LFS cochee) puis relance.
goto end_fail
:no_ignore
echo ERREUR : l'analyse des assets a echoue (voir Saved\Logs\claude_github_full.log).
goto end_fail
:commit_fail
echo ERREUR pendant le commit.
goto end_fail
:no_auth
echo ERREUR : connexion a GitHub non terminee. Relance ce fichier.
goto end_fail
:no_repo
echo ERREUR : creation du depot impossible (un depot "Bata-Z" existe peut-etre deja sur ton compte).
goto end_fail
:push_fail
echo ERREUR pendant l'envoi. Relance ce fichier : l'envoi reprend ou il s'est arrete.
goto end_fail
:end_fail
echo Dis "erreur" a Claude.
pause
exit /b 1
