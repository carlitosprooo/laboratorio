; KaloxSooth 1.3.0 - instalador Windows x64 (NSIS)
; Compilar: makensis -DSRC=<pasta extraida do ZIP> -DOUTDIR=<saida> KaloxSooth.nsi

Unicode true
ManifestDPIAware true
SetCompressor /SOLID lzma
RequestExecutionLevel admin

!define APPNAME     "KaloxSooth"
!define VERSION     "1.3.0"
!define PUBLISHER   "Kalox Audio"
!define UNINST_KEY  "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
!define VST3DIR     "$COMMONFILES64\VST3\${APPNAME}.vst3"

!include "MUI2.nsh"
!include "x64.nsh"
!include "WinVer.nsh"

Name "${APPNAME} ${VERSION}"
OutFile "${OUTDIR}\${APPNAME}_${VERSION}_Windows_x64_Setup.exe"
InstallDir "$PROGRAMFILES64\${PUBLISHER}\${APPNAME}"
BrandingText "${PUBLISHER}"

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=0 "ProductName"     "${APPNAME}"
VIAddVersionKey /LANG=0 "CompanyName"     "${PUBLISHER}"
VIAddVersionKey /LANG=0 "FileDescription" "${APPNAME} ${VERSION} Setup"
VIAddVersionKey /LANG=0 "FileVersion"     "${VERSION}"
VIAddVersionKey /LANG=0 "ProductVersion"  "${VERSION}"
VIAddVersionKey /LANG=0 "LegalCopyright"  "${PUBLISHER}"

!define MUI_ICON   "${SRC}\KaloxSooth.vst3\Plugin.ico"
!define MUI_UNICON "${SRC}\KaloxSooth.vst3\Plugin.ico"
!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.txt"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "PortugueseBR"
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_RESERVEFILE_LANGDLL

LangString SecPlugin_Name  ${LANG_PORTUGUESEBR} "Plug-in VST3 (obrigatorio)"
LangString SecPlugin_Name  ${LANG_ENGLISH}      "VST3 plug-in (required)"
LangString SecManuals_Name ${LANG_PORTUGUESEBR} "Manuais (PT / EN)"
LangString SecManuals_Name ${LANG_ENGLISH}      "Manuals (PT / EN)"
LangString SecPlugin_Desc  ${LANG_PORTUGUESEBR} "Instala KaloxSooth.vst3 em C:\Program Files\Common Files\VST3."
LangString SecPlugin_Desc  ${LANG_ENGLISH}      "Installs KaloxSooth.vst3 to C:\Program Files\Common Files\VST3."
LangString SecManuals_Desc ${LANG_PORTUGUESEBR} "Guia do usuario em portugues e ingles, com atalhos no menu Iniciar."
LangString SecManuals_Desc ${LANG_ENGLISH}      "User guide in Portuguese and English, with Start menu shortcuts."
LangString Need64          ${LANG_PORTUGUESEBR} "O KaloxSooth exige Windows 64 bits (x64)."
LangString Need64          ${LANG_ENGLISH}      "KaloxSooth requires 64-bit Windows (x64)."
LangString NeedWin10       ${LANG_PORTUGUESEBR} "O KaloxSooth exige Windows 10 ou 11."
LangString NeedWin10       ${LANG_ENGLISH}      "KaloxSooth requires Windows 10 or 11."
LangString UninstLink      ${LANG_PORTUGUESEBR} "Desinstalar KaloxSooth"
LangString UninstLink      ${LANG_ENGLISH}      "Uninstall KaloxSooth"

Section "!$(SecPlugin_Name)" SecPlugin
  SectionIn RO
  ; Upgrade: substitui o plug-in inteiro (presets/ajustes ficam em Documentos e %APPDATA%)
  RMDir /r "${VST3DIR}"
  SetOutPath "${VST3DIR}"
  File "${SRC}\KaloxSooth.vst3\desktop.ini"
  File "${SRC}\KaloxSooth.vst3\Plugin.ico"
  SetOutPath "${VST3DIR}\Contents\Resources"
  File "${SRC}\KaloxSooth.vst3\Contents\Resources\moduleinfo.json"
  SetOutPath "${VST3DIR}\Contents\x86_64-win"
  File "${SRC}\KaloxSooth.vst3\Contents\x86_64-win\KaloxSooth.vst3"
  ; Icone da pasta no Explorer
  SetFileAttributes "${VST3DIR}\desktop.ini" HIDDEN|SYSTEM
  SetFileAttributes "${VST3DIR}" SYSTEM

  SetOutPath "$INSTDIR"
  File /oname=README.txt    "${SRC}\README_Windows_PT.txt"
  File /oname=README_EN.txt "${SRC}\README_Windows_EN.txt"
  File "${SRC}\KaloxSooth.vst3\Plugin.ico"
  WriteUninstaller "$INSTDIR\Uninstall ${APPNAME}.exe"

  CreateDirectory "$SMPROGRAMS\${PUBLISHER}"
  CreateShortcut "$SMPROGRAMS\${PUBLISHER}\$(UninstLink).lnk" "$INSTDIR\Uninstall ${APPNAME}.exe"

  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayName"     "${APPNAME} ${VERSION}"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayVersion"  "${VERSION}"
  WriteRegStr   HKLM "${UNINST_KEY}" "Publisher"       "${PUBLISHER}"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayIcon"     "$INSTDIR\Plugin.ico"
  WriteRegStr   HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr   HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall ${APPNAME}.exe"'
  WriteRegStr   HKLM "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall ${APPNAME}.exe" /S'
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "EstimatedSize" 6800
SectionEnd

Section "$(SecManuals_Name)" SecManuals
  SetOutPath "$INSTDIR\Manuals"
  File "${SRC}\Manuals\KaloxSooth Guia do Usuario (Portugues).html"
  File "${SRC}\Manuals\KaloxSooth User Guide (English).html"
  CreateDirectory "$SMPROGRAMS\${PUBLISHER}"
  CreateShortcut "$SMPROGRAMS\${PUBLISHER}\KaloxSooth Guia do Usuario.lnk" "$INSTDIR\Manuals\KaloxSooth Guia do Usuario (Portugues).html"
  CreateShortcut "$SMPROGRAMS\${PUBLISHER}\KaloxSooth User Guide.lnk"      "$INSTDIR\Manuals\KaloxSooth User Guide (English).html"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPlugin}  $(SecPlugin_Desc)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecManuals} $(SecManuals_Desc)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP $(Need64) /SD IDOK
    Abort
  ${EndIf}
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_ICONSTOP $(NeedWin10) /SD IDOK
    Abort
  ${EndIf}
  SetRegView 64
  SetShellVarContext all
  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

Function un.onInit
  SetRegView 64
  SetShellVarContext all
FunctionEnd

; Remove plug-in, manuais e atalhos. Presets (Documentos\SootheKalox) e ajustes (%APPDATA%\SootheKalox) ficam.
Section "Uninstall"
  RMDir /r "${VST3DIR}"
  Delete "$SMPROGRAMS\${PUBLISHER}\$(UninstLink).lnk"
  Delete "$SMPROGRAMS\${PUBLISHER}\Desinstalar KaloxSooth.lnk"
  Delete "$SMPROGRAMS\${PUBLISHER}\Uninstall KaloxSooth.lnk"
  Delete "$SMPROGRAMS\${PUBLISHER}\KaloxSooth Guia do Usuario.lnk"
  Delete "$SMPROGRAMS\${PUBLISHER}\KaloxSooth User Guide.lnk"
  RMDir "$SMPROGRAMS\${PUBLISHER}"
  RMDir /r "$INSTDIR\Manuals"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\README_EN.txt"
  Delete "$INSTDIR\Plugin.ico"
  Delete "$INSTDIR\Uninstall ${APPNAME}.exe"
  RMDir "$INSTDIR"
  RMDir "$PROGRAMFILES64\${PUBLISHER}"
  DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd
