' Start PowerShell hidden before a console window can be displayed.
' MIT License (see: LICENSE), Copyright (C) 2026 Tomaz Stih.
Option Explicit
Dim shell, directory, command
Set shell = CreateObject("WScript.Shell")
directory = CreateObject("Scripting.FileSystemObject").GetParentFolderName(WScript.ScriptFullName)
command = "powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File """ & directory & "\session.ps1"" -Action Run"
WScript.Quit shell.Run(command, 0, True)
