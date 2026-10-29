# KaloxSooth — instalador Windows (NSIS)

Gera `KaloxSooth_1.3.0_Windows_x64_Setup.exe` a partir do ZIP do VST3 (`KaloxSooth_1.3.0_Windows_x64_VST3.zip`).

- Plug-in em `C:\Program Files\Common Files\VST3\KaloxSooth.vst3`
- Manuais, READMEs e desinstalador em `C:\Program Files\Kalox Audio\KaloxSooth\`
- Entrada em "Aplicativos instalados" e atalhos no menu Iniciar (Kalox Audio)
- Português (Brasil) e inglês; exige Windows 10/11 x64 e permissão de administrador
- Desinstalar não apaga presets (`Documentos\SootheKalox`) nem ajustes (`%APPDATA%\SootheKalox`)

## Compilar (Linux ou Mac)

Extraia o ZIP numa pasta (o ZIP vem com separador `\`; use um extrator que converta para `/`) e rode:

```bash
makensis -DSRC=/caminho/do/zip-extraido -DOUTDIR=. KaloxSooth.nsi
```

`apt install nsis` no Linux, `brew install makensis` no Mac. O instalador não tem assinatura de código,
então o SmartScreen pode avisar na primeira execução.
