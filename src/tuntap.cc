#include "tuntap.h"
#include "throwerror.h"
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

Napi::FunctionReference TunInterface::constructor;

void TunInterface::ReadOnlyProperty(const Napi::CallbackInfo &info, const Napi::Value &value) {
    Napi::Env env = info.Env();
    throwTypeError(env, "Property is read-only");
    return;
}

Napi::Function TunInterface::Init(Napi::Env env) {
    Napi::Function func = DefineClass(env, "TunInterface", {
        InstanceMethod("setPersist", &TunInterface::SetPersist),
        InstanceMethod("setAddress", &TunInterface::SetAddress),
        InstanceAccessor("name", &TunInterface::GetName, &TunInterface::ReadOnlyProperty),
        InstanceAccessor("fd", &TunInterface::GetFd, &TunInterface::ReadOnlyProperty)
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    return func;
}

TunInterface::TunInterface(const Napi::CallbackInfo &info) : Napi::ObjectWrap<TunInterface>(info) {
    Napi::Env env = info.Env();
    Napi::Object options;
    if (info.Length() < 1) options = Napi::Object::New(env);
    else {
        if (!info[0].IsObject()) {
            throwTypeError(env, "Argument should be an object");
            return;
        }
        options = info[0].As<Napi::Object>();
    }
    ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    // options.name (string, optional) name of interface
    // default (none) in which kernel allocates next available device
    if (options.Has("name")) {
        if (!options.Get("name").IsString()) {
            throwTypeError(env, "options.name should be a string");
            return;
        }
        std::string name = options.Get("name").As<Napi::String>();
        strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ);
    }
    // options.mode (string, optional) specifies type, default "tun"
    // can be either "tun" or "tap"
    if (!options.Has("mode")) ifr.ifr_flags |= IFF_TUN;
    else {
        if (!options.Get("mode").IsString()) {
            throwTypeError(env, "options.mode should be a string");
            return;
        }
        std::string mode = options.Get("mode").As<Napi::String>();
        if (mode == "tun") ifr.ifr_flags |= IFF_TUN;
        else if (mode == "tap") ifr.ifr_flags |= IFF_TAP;
        else {
            throwTypeError(env, "options.mode must be either 'tun' or 'tap'");
            return;
        }
    }
    // options.pi (boolean, optional) specifies whether or not the 4-byte protocol
    // information header should be prepended to raw packets by the kernel
    // default false
    if (!options.Has("pi")) ifr.ifr_flags |= IFF_NO_PI;
    else {
        if (!options.Get("pi").IsBoolean()) {
            throwTypeError(env, "options.pi should be a boolean");
            return;
        }
        if (!options.Get("pi").As<Napi::Boolean>()) ifr.ifr_flags |= IFF_NO_PI;
    }
    fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        throwError(env, "open /dev/net/tun: " + (std::string)strerror(errno));
        return;
    }
    if (ioctl(fd, TUNSETIFF, &ifr) < 0) {
        throwError(env, "ioctl TUNSETIFF: " + (std::string)strerror(errno));
        return;
    }
    name = std::string(ifr.ifr_name);
}

Napi::Value TunInterface::GetName(const Napi::CallbackInfo &info) {
    return Napi::String::New(info.Env(), name);
}

Napi::Value TunInterface::GetFd(const Napi::CallbackInfo &info) {
    return Napi::Number::New(info.Env(), fd);
}

Napi::Value TunInterface::SetPersist(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return throwTypeError(env, "Needs 1 argument");
    if (!info[0].IsBoolean()) return throwTypeError(env, "Argument should be a boolean");
    int result = ioctl(fd, TUNSETPERSIST, (uintptr_t)(bool)info[0].As<Napi::Boolean>());
    if (result < 0) return throwError(env, "ioctl TUNSETPERSIST: " + (std::string)strerror(errno));
    return info[0];
}

#define ifreq_offsetof(x)  offsetof(struct ifreq, x)

Napi::Value TunInterface::SetAddress(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return throwTypeError(env, "Needs 2 arguments - \"IP\" and \"MASK\"");
    if (!info[0].IsString()) return throwTypeError(env, "Argument should be a string");
    // if (!info[1].IsString()) return throwTypeError(env, "Argument should be a string");

    std::string IP_ADDR = info[0].As<Napi::String>();

    struct ifreq ifr;
    struct sockaddr_in sai;
    int sockfd;
    char *p;

    /* Create a channel to the NET kernel. */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    /* get interface name */
    strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ);

    memset(&sai, 0, sizeof(struct sockaddr));
    sai.sin_family = AF_INET;
    sai.sin_port = 0;

    sai.sin_addr.s_addr = inet_addr(IP_ADDR.c_str());

    p = (char *) &sai;
    memcpy( (((char *)&ifr + ifreq_offsetof(ifr_addr) )),
                    p, sizeof(struct sockaddr));

    ioctl(sockfd, SIOCSIFADDR, &ifr);
    ioctl(sockfd, SIOCGIFFLAGS, &ifr);

    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    // ifr.ifr_flags &= ~selector;  // unset something

    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
        throwError(env, "ioctl SIOCSIFFLAGS: " + (std::string)strerror(errno));
        return info[0];
    }

    // int fd;
    // struct ifreq ifr;
    // struct sockaddr_in *addr;

    // /*AF_INET - to define IPv4 Address type.*/
    // ifr.ifr_addr.sa_family = AF_INET;

    // /*eth0 - define the ifr_name - port name
    // where network attached.*/
    // // memcpy(ifr.ifr_name, name, IFNAMSIZ-1);
    // strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ-1);

    // /*defining the sockaddr_in*/
    // addr=(struct sockaddr_in *)&ifr.ifr_addr;

    // // Napi::String ip;
    // std::string ip = info[0].As<Napi::String>();

    // /*convert ip address in correct format to write*/
    // inet_pton(AF_INET, ip.c_str(), &addr->sin_addr);

    // /*Setting the Ip Address using ioctl*/
    // ioctl(fd, SIOCSIFADDR, &ifr);

    return info[0];
}

TunInterface::~TunInterface() {
    // according to the linux programmer's manual, errors should be able to be
    // silently ignored in this case
    close(fd);
}
