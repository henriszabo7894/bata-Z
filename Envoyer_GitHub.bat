@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title Envoi de Bata-Z sur GitHub
set "PATH=%PATH%;C:\Program Files\Git\cmd"
echo ============================================================
echo  Envoi du jeu sur GitHub (sans code a recopier)
echo  1. Sur github.com : New repository, nom Bata-Z, PRIVATE,
echo     ne coche rien d'autre, puis Create repository.
echo  2. Copie l'adresse https://github.com/.../Bata-Z.git
echo ============================================================
if not exist ".git" goto no_repo
set "REPO_URL="
set /p "REPO_URL=Colle l'adresse du depot puis Entree : "
if "%REPO_URL%"=="" goto no_url
git remote remove origin >nul 2>&1
git remote add origin "%REPO_URL%"
git config credential.helper manager
git add -A
git diff --cached --quiet
if not errorlevel 1 goto push
git commit -q -m "Bata-Z : mise a jour du jeu" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" --trailer "Claude-Session: https://claude.ai/code/session_011BfursW567sQJA8wLpM3ZH"
:push
echo.
echo Une fenetre "Connect to GitHub" va s'ouvrir : clique "Sign in with your browser"
echo puis "Authorize" dans le navigateur. Ensuite l'envoi demarre (plusieurs Go, c'est long).
echo.
git push -u origin main
if errorlevel 1 goto push_fail
echo.
echo  TERMINE : le jeu est sur %REPO_URL%
echo  Dis "fini" a Claude.
pause
exit /b 0

:no_repo
echo ERREUR : lance d'abord Publier_GitHub.bat (il prepare le depot local).
goto end_fail
:no_url
echo ERREUR : aucune adresse collee.
goto end_fail
:push_fail
echo ERREUR pendant l'envoi. Relance ce fichier : l'envoi reprend ou il s'est arrete.
goto end_fail
:end_fail
echo Dis "erreur" a Claude.
pause
exit /b 1
