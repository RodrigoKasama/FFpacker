#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

#include <fcntl.h>
#include <unistd.h>

#include <string.h>

#define TEMP_FILE ".auxiliar.h264"

#define exit_error(...){fprintf(stderr, __VA_ARGS__);exit(1);}

typedef struct{
    int id;
    char *buffer;
    int bufsize;
    int nb_frames;
    double framerate;
    int width;
    int height;
    int vStreamidx;
    int tmp_append;
} tmpinfo_t;

// gcc -o main static_fd_v4_2.c -lavformat -lavutil -lavcodec
// ./main 'rtsp://admin:123456@xxx.yyy.zzz:5540/H264?ch=1&subtype=1' tcp "$PWD" 10

int move_tmpf(tmpinfo_t *tmp_file){

    char filename[40] = {0};
    // Monta as propriedades do arquivo temporario
    sprintf(filename, "File_%d_%.2f_%dx%d_%05d.h264", tmp_file->nb_frames, tmp_file->framerate, tmp_file->width, tmp_file->height, ++(tmp_file->id));
    // fprintf(stdout, "Nome criado: %s\n", filename);
    // Move o bloco para um local com nome das propriedades () CAMINHO FINAL.
    rename(TEMP_FILE, filename);
    return 0;
}

int count_tmp_frames(char* file){

    AVFormatContext *fmt_ctx = NULL;
    AVPacket *pkt;
    av_register_all();
    
    if (avformat_open_input(&fmt_ctx, file, NULL, NULL) < 0) {
        return -1;
    }
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        return -1;
    }

    pkt = av_packet_alloc();
    /* read frames from the file */
    long int video_frame_count = 0;
    while (av_read_frame(fmt_ctx, pkt) >= 0) video_frame_count++;

    avformat_close_input(&fmt_ctx);

    return video_frame_count;
}

double get_framerate(int video_index){

    AVFormatContext *fmt_ctx = NULL;
    AVStream *video_stream = NULL;
    double fps = 0;

    if (!(fmt_ctx = avformat_alloc_context())){
        fprintf(stdout, "Couldn't get input format (rtsp)");
        return -1;
    }
    if (avformat_open_input(&fmt_ctx, TEMP_FILE, NULL, NULL) != 0){
        fprintf(stdout, "Couldn't open file...\n");
        return -1;
    }
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0){
        fprintf(stdout, "Couldn't find stream information\n");
        return -1;
    }
    video_stream = fmt_ctx->streams[video_index];
    if (!video_stream){
        fprintf(stdout, "Couldn't find video stream of the input, aborting\n");
        return -1;
    }

    fps = av_q2d(video_stream->avg_frame_rate);
    avformat_free_context(fmt_ctx);
    return fps;
}

/*
/////////////////////
Escreve com o keyframe no inicio E fim de cada arquivo...
/////////////////////

int write_temp_fil(void *dados, tmpinfo_t *temp_arq, int move_flag){

    static int fd = 0;
    // RESOLVER - Em caso de interrupção, procurar um arquivo temporário incompleto e completá-lo (KEYFRAME) e começar um novo arquivo
    
    static int counter_files = 0;

    if (fd == 0){
        fd = open(TEMP_FILE, O_CREAT | O_WRONLY | O_TRUNC | O_SYNC, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd < 0){
            // USAR perror() 
            exit_error("Erro em instanciar o arquivo temp.\n");
            return -1;
        }
    }

    // exit_error("Fd = %d.\n", fd);

    // Escreve no fd..
    write(fd, ((AVPacket *)dados)->buf->data, ((AVPacket *)dados)->buf->size);
    // Incrementa o contador de frames, de acordo com a escrita
    temp_arq->nb_frames++;

    exit_error("FD=%d\n", fd);
    // Se é para fechar, pega o framerate, fecha e move.
    if (move_flag){

        // Se inverter a ordem desses dois comandos, na proxima abertura ele incrementa, 
        // Pegar o framerate, fechar e mover...
        int fps = get_framerate(temp_arq->vStreamidx);
        if (fps == -1){
            temp_arq->framerate = 0;
        } else {
            temp_arq->framerate = fps;
        }

        close(fd);

        move_tmpf(temp_arq);
        fd = 0;
        // Zera o contador de frames - Arquivo foi movido..
        temp_arq->nb_frames = 0;
    }
    return 0;
}

    // Add apos 'write_temp_file(pPkt, &aux_file, move_file);'
    // Caso escreveu um keyframe, e preciso que o arquivo seguinte comece tambem com um keyframe, por isso escreve novamente sem fechar/mover o tmp
    // Obs: solucao simples porem lógica, verificar se tem um jeito mais unix de fazer
    if (move_file == 1){
        write_temp_file(pPkt, &aux_file, 0);
    }
*/

// Escreve o keyframe apenas no inicio de cada arquivo..
int write_temp_file(tmpinfo_t *temp_arq, int move_flag){

    static int fd = 0;
    /* RESOLVER -
    Em caso de interrupção, procurar um arquivo temporário incompleto e completá-lo (KEYFRAME) e começar um novo arquivo
    */
    static int counter_files = 0;

    // Fd sem informacao -> Cria o arquivo temporario
    if (fd == 0){

        if (temp_arq->tmp_append == 1){
            fd = open(TEMP_FILE, O_WRONLY | O_APPEND);
        } else {
            fd = open(TEMP_FILE, O_CREAT | O_WRONLY | O_TRUNC | O_SYNC, S_IRWXU | S_IRWXG | S_IRWXO);
        }

        if (fd < 0){
            /* USAR perror() */
            fprintf(stderr, "Erro em instanciar o arquivo temp.\n");
            return -1;
        }
    }
    // Se move_flag, Pega o framerate, fecha, move, abre e escreve
    // Senao, so escreve no fd aberto..
    if (move_flag){

        // Se inverter a ordem desses dois comandos (get_framerate() e close()), na proxima abertura o fd incrementa, 
        // Pegar o framerate, fechar e mover...
        int fps = get_framerate(temp_arq->vStreamidx);

        temp_arq->framerate = (fps != -1) ? fps : 0;

        close(fd);

        // Move o arquivo com o novo nome
        move_tmpf(temp_arq);

        // Zera o contador de frames - Arquivo foi movido..
        temp_arq->nb_frames = 0;
        temp_arq->framerate = 0;
        temp_arq->tmp_append = 0;

        // Abre um novo aquivo temp
        // TODO Tentei remover mas fd incrementa ou não consegue ler o framerate
        fd = open(TEMP_FILE, O_CREAT | O_WRONLY | O_TRUNC | O_SYNC, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd < 0){
            /* USAR perror() */
            fprintf(stderr, "Erro em instanciar o arquivo temp.\n");
        }
    }
    // Printa o fd..
    // fprintf(stderr, "Fd = %d.\n", fd);

    // Escreve no fd..
    write(fd, temp_arq->buffer, temp_arq->bufsize);
    // Incrementa o contador de frames, de acordo com a escrita
    temp_arq->nb_frames++;

    return 0;
}

int main(int argc, char **argv){

    if (argc != 5){
        fprintf(stderr, "\n Usage: %s <url> <protocols> <current_path> <timeout-sec>\n\n", argv[0]);
        fprintf(stderr, "   Ex: %s 'rtsp://1.2.3.45/video' tcp '$PWD' 10\n\n", argv[0]);
        fprintf(stderr, "   ** Escrito por: Rodrigo Parracho **\n\n");
        exit(1);
    }

    char *url = argv[1];
    char *protocol = argv[2];
    
    if(chdir(argv[3]) != 0){
        exit_error("Nao foi possivel ir para o diretorio '%s'.\n", argv[3]);
    }

    int vstream_index = -1;

    av_register_all();
    avformat_network_init();
    // av_log_set_level(AV_LOG_ERROR);

    tmpinfo_t aux_file = {0};
    AVInputFormat *in_fmt = NULL;
    AVFormatContext *in_fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVCodec *codec = NULL;
    AVStream *vStream = NULL;
    AVPacket *pPkt = NULL;
    AVDictionary *opts = NULL;

    char timeout[10];
    sprintf(timeout, "%s000000", argv[4]);
    av_dict_set(&opts, "stimeout", timeout, 0);

    // Acessando via protocolo..
    av_dict_set(&opts, "rtsp_transport", protocol, 0);

    if (!(in_fmt_ctx = avformat_alloc_context())){
        exit_error("Couldn't get input format (rtsp)");
    }	
    if (!(in_fmt = av_find_input_format("rtsp"))){
        exit_error("Error guessing input format.\n");
    }

    fprintf(stdout, "Trying transport protocol %s.. \n", protocol);
    if (avformat_open_input(&in_fmt_ctx, url, in_fmt, &opts) != 0){
        exit_error("Failed on connection using %s.\n", protocol);
    }

    fprintf(stdout, "Connection succeded.\n Protocol Used: %s.\nFinding stream infomation...\n", protocol);

    if (avformat_find_stream_info(in_fmt_ctx, NULL) < 0){
        exit_error("Couldn't find stream information\n");
    }

    // Encontra o indice da stream dentro do container
    fprintf(stdout, "Finding stream index...\n");
    int ret = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0){
        exit_error("Couldn't find %s stream in input file.\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
    }

    vstream_index = ret;

    fprintf(stdout, "Creating stream instance...\n");
    vStream = in_fmt_ctx->streams[vstream_index];
    if (!vStream){
        exit_error("Couldn't find video stream of the input, aborting\n");
    }

    aux_file.width = vStream->codec->width;
    aux_file.height = vStream->codec->height;
    aux_file.vStreamidx = vstream_index;

    /*
    // Adição do AVCodecParameters
        fprintf(stdout, "Getting codec parameters...\n");
        codec_param = vStream->codecpar;
        if (!codec_param) {
            exit_error("Couldn't define Codec Parameters");
        }
    */

    fprintf(stdout, "Finding video decoder...\n");
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec){
        exit_error("Failed to find %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        // return AVERROR(EINVAL);
    }

    codec_ctx = avcodec_alloc_context3(codec);
    /* Find decoder for the stream */
    if (!codec_ctx){
        // exit_error("Couldn't create a Codec Context with Codec Parameters\n");
        exit_error("Failed to copy %s codec parameters to decoder context\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
    }

    /*
    // Para versões mais novas..
    fprintf(stdout, "Filling decoder parameters...\n");
    if (avcodec_parameters_to_context(codec_ctx, codec_param) < 0) {
        exit_error("Erro");
    }
    */

    fprintf(stdout, "Opening stream with decoder %s...\n", codec->name);
    if ((ret = avcodec_open2(codec_ctx, codec, &opts)) < 0){
        exit_error("Failed to open %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
    }

    pPkt = av_packet_alloc();

    // if (temp_frames == -1){
    //     // exit_error("Erro ao ler o arquivo temporario..\n");
    //     aux_file.nb_frames = 0;
    // }
    int temp_frames = count_tmp_frames(TEMP_FILE);
    if (temp_frames > 0){
        fprintf(stderr, "No arquivo temporario foram encontrados %d frames, completando arquivo...\n", temp_frames);
        aux_file.nb_frames = temp_frames;
        aux_file.tmp_append = 1;
    } else {
        fprintf(stderr, "Nao foram encontrados frames no arquivo temporario..\n");
        aux_file.nb_frames = 0;
        aux_file.tmp_append = 0;
    }

    int flag = 0, move_file = 0;

    while (av_read_frame(in_fmt_ctx, pPkt) >= 0){

        if (pPkt->stream_index == vstream_index){

            aux_file.buffer = (char *)pPkt->buf->data;
            aux_file.bufsize = pPkt->buf->size;

            if (!(pPkt->flags == AV_PKT_FLAG_KEY && flag != 0)){
                move_file = 0;
                flag = 1;
            }else{
                move_file = 1;
            }
            write_temp_file(&aux_file, move_file);
        }
        av_packet_unref(pPkt);
    }

    av_packet_free(&pPkt);
    avcodec_free_context(&codec_ctx);
    avformat_free_context(in_fmt_ctx);

    return 0;
}