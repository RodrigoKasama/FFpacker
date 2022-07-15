# MOVIEIT-FFmpeg

- Tarefa 3 - Forma n arquivos diferentes .h264 com o numero de frames, framerate, dimensões.

gcc -o main rtsp_try.c -lavformat -lavutil -lavcodec
./rtsp_try 'rtsp://admin:123456@movplay.com.br:5540/H264?ch=1&subtype=1' tcp,udp 5
