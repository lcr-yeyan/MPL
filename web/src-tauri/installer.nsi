; MPL Studio NSIS installer.
; Built with local NSIS to avoid the Tauri bundler's blocked GitHub tool download.
Unicode true
!include "MUI2.nsh"

!define APP_NAME "MPL Studio"
!define APP_VERSION "0.1.0"
!define APP_EXE "mpl_studio.exe"

Name "${APP_NAME}"
OutFile "..\dist-tauri\MPL-Studio_${APP_VERSION}_x64-setup.exe"
InstallDir "$LOCALAPPDATA\Programs\${APP_NAME}"
InstallDirRegKey HKCU "Software\MPL Studio" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma

VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey "ProductName" "${APP_NAME}"
VIAddVersionKey "FileDescription" "Local-first editor for Music Production Language"
VIAddVersionKey "FileVersion" "${APP_VERSION}"
VIAddVersionKey "ProductVersion" "${APP_VERSION}"
VIAddVersionKey "LegalCopyright" "MPL"

!define MUI_ABORTWARNING
!define MUI_ICON "icons\icon.ico"
!define MUI_UNICON "icons\icon.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Launch MPL Studio"
!define MUI_WELCOMEPAGE_TITLE "MPL Studio"
!define MUI_WELCOMEPAGE_TEXT "A local-first editor for the MPL music production language.$\r$\n$\r$\nIncludes the MPL compiler and standard library; no network or model dependency."

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"

Section "MPL Studio" SecMain
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /r "target\package\staging\*.*"

    WriteUninstaller "$INSTDIR\uninstall.exe"

    CreateDirectory "$SMPROGRAMS\MPL Studio"
    CreateShortCut "$SMPROGRAMS\MPL Studio\MPL Studio.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
    CreateShortCut "$SMPROGRAMS\MPL Studio\Uninstall.lnk" "$INSTDIR\uninstall.exe"
    CreateShortCut "$DESKTOP\MPL Studio.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0

    WriteRegStr HKCU "Software\MPL Studio" "InstallDir" "$INSTDIR"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\MPLStudio" "DisplayName" "${APP_NAME}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\MPLStudio" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\MPLStudio" "Publisher" "MPL"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\MPLStudio" "DisplayIcon" "$INSTDIR\${APP_EXE}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\MPLStudio" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\MPLStudio" "InstallLocation" "$INSTDIR"
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\MPLStudio" "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\MPLStudio" "NoRepair" 1
SectionEnd

Section "Uninstall"
    Delete "$DESKTOP\MPL Studio.lnk"
    Delete "$SMPROGRAMS\MPL Studio\MPL Studio.lnk"
    Delete "$SMPROGRAMS\MPL Studio\Uninstall.lnk"
    RMDir "$SMPROGRAMS\MPL Studio"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\MPLStudio"
    DeleteRegKey HKCU "Software\MPL Studio"
    RMDir /r "$INSTDIR"
SectionEnd
