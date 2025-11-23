# FFpacker

Repositório que apresenta um utilitário (versão 5) para empacotamento de fluxos de vídeo utilizando a biblioteca **libav**.

Com essa biblioteca é possível manipular fluxos multimídia via linha de comando ou por meio de código, permitindo o processamento e armazenamento de transmissões em tempo real.

Objetivo

A partir de uma entrada de vídeo via protocolo RTSP (Real-Time Streaming Protocol), o programa realiza:

- Captura contínua do fluxo de vídeo;

- Segmentação do stream em arquivos contendo uma quantidade fixa de frames, definida via linha de comando;

- Geração de arquivos cujos nomes incluem informações relevantes como:

- Taxa de quadros (framerate)

- Largura e altura do vídeo

- Indicador de interrupção da transmissão


Compilação
---

Para compilar, é necessário possuir as bibliotecas da libav instaladas e vincular o binário conforme abaixo:

``` [bash]
gcc -o ffpacker code.c -lavformat -lavutil -lavcodec
```

Execução
---
``` [bash]
./ffpacker 'rtsp://xxx.yyy.zzz' tcp "$PWD" 10
```