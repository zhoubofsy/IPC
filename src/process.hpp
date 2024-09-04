#ifndef _PROCESS_
#define _PROCESS_

class IOMulti {
    public:
        virtual int multi_poll(int sock) = 0;
};

class NormalIOMulti : public IOMulti {
    private:
        int client_sock;
    public:
        ~NormalIOMulti() {
            close(client_sock);
        }
        int multi_poll(int sock) override {
            struct sockaddr_in c_addr = {0};
            int c_addr_size = sizeof(c_addr);
            client_sock = accept(sock, (struct sockaddr*)&c_addr, (socklen_t*)&c_addr_size);
            return client_sock;
        }
};

class SelectIOMulti : public IOMulti {
    private:
        int client_sock;
    public:
        ~SelectIOMulti() {
            close(client_sock);
        }
        int multi_poll(int sock) override {
            struct sockaddr_in c_addr = {0};
            int c_addr_size = sizeof(c_addr);
            int c_sock = accept(sock, (struct sockaddr*)&c_addr, (socklen_t*)&c_addr_size);
            client_sock = c_sock;

            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(c_sock, &rfds);

            int ret = select(c_sock + 1, &rfds, NULL, NULL, NULL);
            if(ret == -1)
                cout << "select error !" << endl;
            else if(ret == 0)
                cout << "select timeout !" << endl;
            else {
                if(FD_ISSET(c_sock, &rfds))
                    client_sock = c_sock;
            }
            return client_sock;
        }
};

class PollIOMulti : public IOMulti {
    private:
        int client_sock;
    public:
        ~PollIOMulti() {
            close(client_sock);
        }
        int multi_poll(int sock) override {
            struct sockaddr_in c_addr = {0};
            int c_addr_size = sizeof(c_addr);
            int c_sock = accept(sock, (struct sockaddr*)&c_addr, (socklen_t*)&c_addr_size);
            client_sock = c_sock;

            struct pollfd rfds;
            rfds.fd = c_sock;
            rfds.events = POLLIN;
            int ret = poll(&rfds, 1, -1);
            if(ret > 0) {
                if((rfds.revents & POLLIN) == POLLIN)
                    client_sock = rfds.fd;
            } else if(ret == -1)
                cout << "poll error !" << endl;
            else
                cout << "poll timeout !" << endl;
            return client_sock;
        }
};

class EpollIOMulti : public IOMulti {
    private:
        int client_sock;
    public:
        ~EpollIOMulti() {
            close(client_sock);
        }
        int multi_poll(int sock) override {
            struct sockaddr_in c_addr = {0};
            int c_addr_size = sizeof(c_addr);
            int c_sock = accept(sock, (struct sockaddr*)&c_addr, (socklen_t*)&c_addr_size);
            client_sock = c_sock;

            struct epoll_event event = {0};
            int epfd = epoll_create(1);
            event.data.fd = c_sock;
            event.events = EPOLLIN;
            epoll_ctl(epfd, EPOLL_CTL_ADD, c_sock, &event);
            
            struct epoll_event w_event = {0};
            int ret = epoll_wait(epfd, &w_event, 1, -1);
            if(ret > 0) {
                if(w_event.data.fd == c_sock && (w_event.events & EPOLLIN) == EPOLLIN) {
                    client_sock = w_event.data.fd;
                }
            } else if(ret == -1)
                cout << "epoll error !" << endl;
            else
                cout << "epoll timeout !" << endl;

            return client_sock;
        }
};


class IPC {
    public:
        virtual int open() = 0;
        virtual int close() = 0;
        virtual int write(void *buf, const int len) = 0;
        virtual int read(void *buf, const int len) = 0;
};

/*
 * PIPE 单工通讯
 * */
class PipeIPC : public IPC {
    private:
        int fd[2];
    public:
        int open() override {
            pipe(fd);
            return 0;
        }
        int close() override {
            ::close(fd[0]);
            ::close(fd[1]);
            return 0;
        }
        int write(void *buf, const int len) override {
            return ::write(fd[1], buf, len);
        }
        int read(void *buf, const int len) override {
            return ::read(fd[0], buf, len);
        }
};

/*
 * FIFO 单工通讯
 * */
class FifoIPC : public IPC {
    private:
        char const *fifo_name = "fifoipc";
        int w_fd, r_fd;
    public:
        int open() override {
            return mkfifo(fifo_name, 0666);
        }
        int close() override {
            ::close(r_fd);
            ::close(w_fd);
            return 0;
        }
        int write(void *buf, const int len) override {
            w_fd = ::open(fifo_name, O_WRONLY);
            ::write(w_fd, buf, len);
            for(int i = 0; i< 2 ; i++){
                char tmpbuf[40] = {0};
                ::read(w_fd, tmpbuf, 40);
                printf("Writer read : %s\n", tmpbuf);
            }
            return 0;
        }
        int read(void *buf, const int len) override {
            r_fd = ::open(fifo_name, O_RDONLY);
            sleep(5);
            return ::read(r_fd, buf, len);
        }
};

/*
 * mmap 相当于半双工通讯
 * */
class MmapIPC : public IPC {
    private:
        pthread_mutex_t mtx;
        pthread_mutexattr_t mtxattr;
        void *mm;
        const int mm_size = 1024;
    public:
        int open() override {
            // create mutex
            pthread_mutexattr_init(&mtxattr);
            pthread_mutexattr_setpshared(&mtxattr, PTHREAD_PROCESS_SHARED);
            pthread_mutex_init(&mtx, &mtxattr);

            // mmap
            mm = mmap(NULL, mm_size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);
            memset(mm, 0, mm_size);
        }
        int close() override {
            pthread_mutex_destroy(&mtx);
            pthread_mutexattr_destroy(&mtxattr);
            munmap(mm, mm_size);
        }
        int write(void *buf, const int len) override {
            cout << "writing" << endl;
            pthread_mutex_lock(&mtx);
            memcpy(mm, buf, len);
            pthread_mutex_unlock(&mtx);
            return len;
        }
        int read(void *buf, const int len) override {
            cout << "reading" << endl;
            pthread_mutex_lock(&mtx);
            memcpy(buf, mm, len);
            pthread_mutex_unlock(&mtx);
            return len;
        }
};

/*
 * socket 为全双工通讯
 * */
class SocketIPC : public IPC {
    private:
        char const *ip = "127.0.0.1";
        int port = 9527;
        struct sockaddr_in addr = {0};
    public:
        int open() override {
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr(ip);
            addr.sin_port = htons(port);
            return 0;
        }
        int close() override {
            return 0;
        }
        int write(void *buf, const int len) override {
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            while(0 != connect(sockfd, (struct sockaddr*)&addr, sizeof(addr))){
                cout << "Wait for connnect..." << endl;
            } 
            ::write(sockfd, (char*)buf, len);
            // sleep(1);
            // for(int i = 0; i < 2; i++){
            //     char tmpbuf[40] = {0};
            //     ::read(sockfd, (void*)tmpbuf, 40);
            //     printf("Writer read : %s\n", tmpbuf);
            // }
            ::close(sockfd);
        }
        int read(void *buf, const int len) override {
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
            listen(sockfd, 2);
            //IOMulti *io = new NormalIOMulti();
            //IOMulti *io = new SelectIOMulti();
            //IOMulti *io = new PollIOMulti();
            IOMulti *io = new EpollIOMulti();
            int c_sockfd = io->multi_poll(sockfd);

            ::read(c_sockfd, (char*)buf, len);
            // ::write(c_sockfd, "dididi", 6);
            // sleep(5);

            delete io;
            ::close(sockfd);
        }
};

#endif
