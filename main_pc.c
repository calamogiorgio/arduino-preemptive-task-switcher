#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>

#define BUFFER_SIZE 256

/**
 * Configures the serial port interface in raw POSIX compliant mode
 */
int init_serial(void) {
    int ret;
    
    /* Open the serial port device file in Read/Write mode */
    int fd = open("/dev/ttyACM0", O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("Error opening serial port");
        return -1;
    }

    struct termios tty;
    
    /* Get current serial port attributes */
    ret = tcgetattr(fd, &tty);
    if (ret == -1) {
        perror("Error getting attributes (tcgetattr)");
        close(fd);
        return -1;
    }

    /* Set baud rate to 19200 bps for both input and output */
    cfsetispeed(&tty, B19200);
    cfsetospeed(&tty, B19200);

    /* Disable software flow control and byte translations on the wire */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);

    /* Disable post-processing/translations of output bytes */
    tty.c_oflag &= ~OPOST;

    /* Disable canonical mode, echo, and signal processing on serial line */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    
    /* Set format: 8 data bits, no parity, 1 stop bit (8N1) */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    
    /* Enable receiver and local status flags */
    tty.c_cflag |= (CLOCAL | CREAD);
    
    /* Non-blocking read parameters for select() engine operations */
    tty.c_cc[VMIN]  = 0; 
    tty.c_cc[VTIME] = 0; 

    /* Flush stale hardware buffers */
    tcflush(fd, TCIFLUSH);

    /* Apply configurations immediately */
    ret = tcsetattr(fd, TCSANOW, &tty);
    if (ret == -1) {
        perror("Error setting attributes (tcsetattr)");
        close(fd);
        return -1;
    }

    return fd;
}

int main(void) {
    /* Initialize serial line connectivity descriptors */
    int fd = init_serial();
    if (fd == -1) {
        exit(EXIT_FAILURE);
    }

    char uart_buffer[BUFFER_SIZE];
    int uart_index = 0;
    char kbd_buffer[BUFFER_SIZE];

    printf("Listening for Arduino logs. Type a line and press ENTER to send it...\n");

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);           /* Monitor Arduino incoming hardware events */
        FD_SET(STDIN_FILENO, &read_fds);  /* Monitor PC console keyboard input (canonical) */

        /* Block thread context execution until at least one resource is ready */
        int activity = select(fd + 1, &read_fds, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR) {
            perror("Select infrastructure framework error");
            break;
        }

        /* Handle incoming stream events sent by Arduino microkernel */
        if (FD_ISSET(fd, &read_fds)) {
            char ch;
            if (read(fd, &ch, 1) > 0) {
                if (uart_index < BUFFER_SIZE - 1) {
                    uart_buffer[uart_index++] = ch;
                }

                /* End of log sentence captured */
                if (ch == '\n') {
                    uart_buffer[uart_index] = '\0';
                    
                    /* Sanitize trailing line breaks */
                    while (uart_index > 0 && (uart_buffer[uart_index - 1] == '\n' || uart_buffer[uart_index - 1] == '\r')) {
                        uart_buffer[--uart_index] = '\0';
                    }
                    
                    if (uart_index > 0) {
                        printf("Received data: %s\n", uart_buffer);
                        fflush(stdout);
                    }
                    uart_index = 0;
                }
            }
        }

        /* Handle user keyboard entries upon hitting ENTER */
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(kbd_buffer, sizeof(kbd_buffer), stdin) != NULL) {
                int len = strlen(kbd_buffer);
                
                /* Flush full line chunk data to hardware TX pipe */
                write(fd, kbd_buffer, len);
                
                printf("[PC Sent]: %s", kbd_buffer);
                fflush(stdout);
            }
        }
    }

    /* Tear down resources safely */
    close(fd);
    return 0;
}