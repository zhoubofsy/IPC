#include "unistd.h"
#include "sys/wait.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "iostream"
#include "sys/mman.h"
#include "pthread.h"
#include "string.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "arpa/inet.h"
#include "sys/select.h"
#include "poll.h"
#include "sys/epoll.h"

using namespace std;

#include "process.hpp"


class Fork {
    public:
        virtual int parentHandle() = 0;
        virtual int childHandle() = 0;
        virtual int doFork() = 0;
        virtual int waitFinish() = 0;
};

class BasicFork : public Fork {
    private:
        pid_t pid_child;
    public:
        int doFork() final {
            pid_t pid = fork();
            if((int)pid == 0) {
                // child
                this->childHandle();
                exit(0);
            } else if((int)pid > 0) {
                // parent
                pid_child = pid;
                return this->parentHandle();
            }
        }

        int waitFinish() final {
            if(pid_child == 0) {
                cout << "Child wait finish" << endl;
                return 0;
            }
            return (int)waitpid(pid_child, NULL, 0);
        }
};

class TestFork : public BasicFork {
    public:
        int parentHandle() override {
            cout << "This ParentHandle !" << endl;
            return 0;
        }
        int childHandle() override {
            cout << "This ChildHandle !" << endl;
            return 0;
        }
};

class IPCFork : public BasicFork {
    private:
        IPC *ipc;
    public:
        IPCFork(IPC *c) : ipc(c) {
            this->ipc->open();
        }
        ~IPCFork() {
            this->ipc->close();
        }
        int parentHandle() override {
            int w_num = 0;
            char const *say = "I love you";
            w_num = this->ipc->write((void*)say, strlen(say));
            return w_num;
        }
        int childHandle() override {
            char hear[20] = {0};
            this->ipc->read((void*)hear, 20);
            printf("I hear : %s\n", hear);
            return strlen(hear);
        }
};

int main() {

    //Fork *fk = new TestFork();
    
    // Picp IPC
    //IPC *c = new PipeIPC();
    // FIFO IPC
    //IPC *c = new FifoIPC();
    // Mmap IPC
    //IPC *c = new MmapIPC();
    // Socket IPC
    IPC *c = new SocketIPC();
    
    Fork *fk = new IPCFork(c);

    fk->doFork();
    fk->waitFinish();

    cout << "Done!" << endl;

    delete fk;
    delete c;

    return 0;
}
