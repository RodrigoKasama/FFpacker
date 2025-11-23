#include <unistd.h>
#include <fcntl.h>

void funcao() {
    static int i = 0;
    i++;
    printf("Contador %d - ", i);
}

void write_temp_file(char *payload, int limit) {
    
	static int fd = -1;
    static int counter = 0;
    static int contador_de_arquivos = 0;

    static char *temp_dir = "/tmp/teste.txt";
    char filename[20];

    if (fd < 0) {
        fd = open(temp_dir, O_RDWR | O_CREAT);
    }

    printf("%d\n", fd);

    if (counter == limit) {
        if (fd > 0)
            close(fd);

        sprintf(filename, "char_com_%d_%d.txt", counter + 1, contador_de_arquivos + 1);

        rename(temp_dir, filename);

        counter = 0;
        contador_de_arquivos++;

        fd = open(temp_dir, O_RDWR | O_CREAT);

        if (fd > 0) {
            write(fd, payload, 7 * sizeof(char));
        }
    } else {
        write(fd, payload, 7 * sizeof(char));
        counter++;
    }
}

int main(void)
{
    while (1)
    {
        funcao();
        write_temp_file("lalala\n", 10);
        sleep(1);
    }
}
