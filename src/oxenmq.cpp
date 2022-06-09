#include <napi.h>
#include <iostream>
#include <oxenmq/connections.h>
#include <oxenmq/oxenmq.h>

namespace noxenmq {

namespace {
    template <typename T, typename F>
    T napi_cast(const Napi::CallbackInfo& info, F&& value) {
        return T::New(info.Env(), std::forward<F>(value));
    }

    template <typename F>
    Napi::Buffer<uint8_t> napi_copy_bytes(const Napi::Env& env, const F& value) {
        static_assert(sizeof(*value.data()) == 1);
        return Napi::Buffer<uint8_t>::Copy(
                env, reinterpret_cast<const uint8_t*>(value.data()), value.size());
    }
}  // namespace

struct ctors {
    Napi::FunctionReference NConnectionID, NAddress, NMessage, NOxenMQ;
};

class NConnectionID : public Napi::ObjectWrap<NConnectionID> {
  private:
    std::unique_ptr<oxenmq::ConnectionID> instance;

  public:
    NConnectionID(const Napi::CallbackInfo& info) : Napi::ObjectWrap<NConnectionID>(info) {
        if (info.Length() != 1 || !info[0].IsExternal())
            throw std::logic_error{"ConnectionID is not directly constructible"};

        instance.reset(info[0].As<Napi::External<oxenmq::ConnectionID>>().Data());
    }

    static void napi_init(Napi::Env& env, Napi::Object& exports, ctors& ct) {
        Napi::Function func = DefineClass(
                env,
                "ConnectionID",
                {
                        InstanceMethod("isServiceNode", &NConnectionID::isServiceNode),
                        InstanceMethod("pubkey", &NConnectionID::pubkey),
                        InstanceMethod("equals", &NConnectionID::equals),
                });

        ct.NConnectionID = Napi::Persistent(func);

        exports.Set("ConnectionID", func);
    }

    const oxenmq::ConnectionID& get() const { return *instance; }

    Napi::Value isServiceNode(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::Boolean>(info, instance->sn());
    }

    Napi::Value pubkey(const Napi::CallbackInfo& info) {
        return napi_copy_bytes(info.Env(), instance->pubkey());
    }

    Napi::Value equals(const Napi::CallbackInfo& info) {
        auto env = info.Env();
        Napi::HandleScope scope{env};

        if (info.Length() != 1)
            throw std::invalid_argument{"equals() requires a ConnectionID argument"};

        auto* other = Unwrap(info[0].As<Napi::Object>());
        return Napi::Boolean::New(env, instance == other->instance);
    }

    // Copy a oxenmq::ConnectionID into a node-wrapped NConnectionID
    static Napi::Object wrap(const Napi::Env& env, const oxenmq::ConnectionID& conn) {
        return env.GetInstanceData<ctors>()->NConnectionID.New(
                {Napi::External<oxenmq::ConnectionID>::New(
                        env, new oxenmq::ConnectionID{conn})});
    }

    friend class NMessage;
};

class NAddress : public Napi::ObjectWrap<NAddress> {
  private:
    oxenmq::address instance;

  public:
    NAddress(const Napi::CallbackInfo& info) : Napi::ObjectWrap<NAddress>(info) {
        auto env = info.Env();
        Napi::HandleScope scope{env};

        size_t length = info.Length();

        // Single string argument: construct an address from an encoded address such as
        // "curve://HOSTNAME:PORT/PUBKEY".
        if (length == 1 && info[0].IsString()) {
            std::string addr = info[0].ToString();
            instance = oxenmq::address{addr};
            return;
        }

        throw std::invalid_argument{
                "Invalid arguments: Address must be constructed with a single address string"};
    }
    static void napi_init(Napi::Env& env, Napi::Object& exports, ctors& ct) {
        Napi::HandleScope scope{env};

        Napi::Function func = DefineClass(
                env,
                "Address",
                {
                        InstanceAccessor("pubkey", &NAddress::pubkey, nullptr),
                        InstanceAccessor("curve", &NAddress::curve, nullptr),
                        InstanceAccessor("tcp", &NAddress::tcp, nullptr),
                        InstanceAccessor("ipc", &NAddress::ipc, nullptr),
                        InstanceAccessor("zmqAddress", &NAddress::zmqAddress, nullptr),
                        InstanceAccessor("fullAddress", &NAddress::fullAddress, nullptr),
                        InstanceAccessor("fullAddressB64", &NAddress::fullAddressB64, nullptr),
                        InstanceAccessor("fullAddressHex", &NAddress::fullAddressHex, nullptr),
                        InstanceAccessor("qr", &NAddress::qr, nullptr),
                        InstanceMethod("equals", &NAddress::equals),
                });

        ct.NAddress = Napi::Persistent(func);

        exports.Set("Address", func);
    }

    const oxenmq::address& get() const { return instance; }

    Napi::Value pubkey(const Napi::CallbackInfo& info) {
        return napi_copy_bytes(info.Env(), instance.pubkey);
    }
    Napi::Value curve(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::Boolean>(info, instance.curve());
    }
    Napi::Value tcp(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::Boolean>(info, instance.tcp());
    }
    Napi::Value ipc(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::Boolean>(info, instance.ipc());
    }
    Napi::Value zmqAddress(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::String>(info, instance.zmq_address());
    }
    Napi::Value fullAddress(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::String>(
                info, instance.full_address(oxenmq::address::encoding::base32z));
    }
    Napi::Value fullAddressB64(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::String>(
                info, instance.full_address(oxenmq::address::encoding::base64));
    }
    Napi::Value fullAddressHex(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::String>(info, instance.full_address(oxenmq::address::encoding::hex));
    }
    Napi::Value qr(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::String>(info, instance.qr_address());
    }
    Napi::Value equals(const Napi::CallbackInfo& info) {
        auto env = info.Env();
        Napi::HandleScope scope{env};

        if (info.Length() != 1)
            throw std::invalid_argument{"equals() requires an Address argument"};

        auto* other = Unwrap(info[0].As<Napi::Object>());
        return Napi::Boolean::New(env, instance == other->instance);
    }

    // DEBUG:
    Napi::Value wtf(const Napi::CallbackInfo& info) {
        auto env = info.Env();
        auto obj = env.GetInstanceData<ctors>()->NAddress.New(
                {Napi::String::New(env, instance.full_address())});
        return obj;
    }
};

// oxenmq::Message is designed to be ephemeral, used only in the callback.  That doesn't fit well at
// all with node, so we effectively copy the important data out of it.
class NMessage : public Napi::ObjectWrap<NMessage> {
  private:
    std::string remote_;
    Napi::TypedArrayOf<Napi::Buffer<uint8_t>> data_;
    // Holds the conn and reply tag for us; we don't currently use it to send things, but we could
    // in the future.
    std::optional<oxenmq::Message::DeferredSend> defer_;

  public:
    NMessage(const Napi::CallbackInfo& info) : Napi::ObjectWrap<NMessage>(info) {
        if (info.Length() != 1 || !info[0].IsExternal())
            throw std::logic_error{"Message is not directly constructible"};
        std::unique_ptr<oxenmq::Message> mptr{info[0].As<Napi::External<oxenmq::Message>>().Data()};
        auto env = info.Env();
        auto& m = *mptr;
        defer_.emplace(m.send_later());
        remote_ = std::move(m.remote);
        data_ = Napi::TypedArrayOf<Napi::Buffer<uint8_t>>::New(env, m.data.size());
        for (size_t i = 0; i < m.data.size(); i++)
            data_[i] = napi_copy_bytes(env, m.data[i]);
    }

    static void napi_init(Napi::Env& env, Napi::Object& exports, ctors& ct) {
        Napi::HandleScope scope{env};

        Napi::Function func = DefineClass(
                env,
                "Message",
                {
                        InstanceAccessor("isReply", &NMessage::isReply, nullptr),
                        InstanceAccessor("remoteAddress", &NMessage::remoteAddress, nullptr),
                        InstanceAccessor("conn", &NMessage::conn, nullptr),
                        // Returns a *copy* of the data; oxenmq exposes a view, but it's only valid
                        // for the duration of the callback itself.
                        InstanceAccessor("data", &NMessage::data, nullptr),
                });

        ct.NMessage = Napi::Persistent(func);

        exports.Set("Message", func);
    }

    Napi::Value isReply(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::Boolean>(info, !defer_->reply_tag.empty());
    }
    Napi::Value remoteAddress(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::String>(info, remote_);
    }
    Napi::Value conn(const Napi::CallbackInfo& info) {
        auto env = info.Env();
        auto c = env.GetInstanceData<ctors>()->NConnectionID.New(
                {Napi::External<oxenmq::ConnectionID>::New(
                        env, new oxenmq::ConnectionID{defer_->conn})});
        return c;
    }
    Napi::Value data(const Napi::CallbackInfo&) { return data_; }
};

class NOxenMQ : public Napi::ObjectWrap<NOxenMQ> {
    std::optional<oxenmq::OxenMQ> omq;

  public:
    NOxenMQ(const Napi::CallbackInfo& info) : Napi::ObjectWrap<NOxenMQ>(info) {
        // TODO: support various constructor invocations to map some of the things the OxenMQ
        // constructor supports.
        if (info.Length() != 0)
            throw std::logic_error{"OxenMQ constructor takes no arguments"};

        omq.emplace();
    }

    static void napi_init(Napi::Env& env, Napi::Object& exports, ctors& ct) {
        Napi::HandleScope scope{env};

        Napi::Function func = DefineClass(
                env,
                "OxenMQ",
                {
                        InstanceAccessor(
                                "handshakeMs", &NOxenMQ::handshake_time, &NOxenMQ::handshake_time),
                        InstanceAccessor(
                                "maxMessageSize", &NOxenMQ::max_msg_size, &NOxenMQ::max_msg_size),
                        InstanceAccessor(
                                "maxSockets", &NOxenMQ::max_sockets, &NOxenMQ::max_sockets),
                        InstanceAccessor(
                                "reconnectIntervalMs",
                                &NOxenMQ::reconnect_interval,
                                &NOxenMQ::reconnect_interval),
                        InstanceAccessor(
                                "maxReconnectIntervalMs",
                                &NOxenMQ::reconnect_interval_max,
                                &NOxenMQ::reconnect_interval_max),
                        InstanceAccessor(
                                "closeLingerMs", &NOxenMQ::close_linger, &NOxenMQ::close_linger),
                        InstanceAccessor(
                                "connCheckIntervalMs", &NOxenMQ::conn_check, &NOxenMQ::conn_check),
                        InstanceAccessor(
                                "connHeartbeatMs",
                                &NOxenMQ::conn_heartbeat,
                                &NOxenMQ::conn_heartbeat),
                        InstanceAccessor(
                                "connHeartbeatTimeoutMs",
                                &NOxenMQ::conn_heartbeat_timeout,
                                &NOxenMQ::conn_heartbeat_timeout),
                        InstanceAccessor("pubkey", &NOxenMQ::pubkey, nullptr),
                        InstanceAccessor("privkey", &NOxenMQ::privkey, nullptr),
                        InstanceMethod("start", &NOxenMQ::start),
                        InstanceMethod("connectRemote", &NOxenMQ::connect_remote),

                });

        ct.NOxenMQ = Napi::Persistent(func);

        exports.Set("OxenMQ", func);
    }

    void handshake_time(const Napi::CallbackInfo&, const Napi::Value& value) {
        omq->HANDSHAKE_TIME = std::chrono::milliseconds{value.As<Napi::Number>().Int64Value()};
    }
    Napi::Value handshake_time(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::Number>(info, omq->HANDSHAKE_TIME.count());
    }
    void max_msg_size(const Napi::CallbackInfo&, const Napi::Value& value) {
        omq->MAX_MSG_SIZE = value.As<Napi::Number>().Int64Value();
    }
    Napi::Value max_msg_size(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::Number>(info, omq->MAX_MSG_SIZE);
    }
    void max_sockets(const Napi::CallbackInfo&, const Napi::Value& value) {
        omq->MAX_SOCKETS = value.As<Napi::Number>().Int32Value();
    }
    Napi::Value max_sockets(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::Number>(info, omq->MAX_SOCKETS);
    }
    void reconnect_interval(const Napi::CallbackInfo&, const Napi::Value& value) {
        omq->RECONNECT_INTERVAL = std::chrono::milliseconds{value.As<Napi::Number>().Int64Value()};
    }
    Napi::Value reconnect_interval(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::Number>(info, omq->RECONNECT_INTERVAL.count());
    }
    void reconnect_interval_max(const Napi::CallbackInfo&, const Napi::Value& value) {
        omq->RECONNECT_INTERVAL_MAX =
                std::chrono::milliseconds{value.As<Napi::Number>().Int64Value()};
    }
    Napi::Value reconnect_interval_max(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::Number>(info, omq->RECONNECT_INTERVAL_MAX.count());
    }
    void close_linger(const Napi::CallbackInfo&, const Napi::Value& value) {
        omq->CLOSE_LINGER = std::chrono::milliseconds{value.As<Napi::Number>().Int64Value()};
    }
    Napi::Value close_linger(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::Number>(info, omq->CLOSE_LINGER.count());
    }
    void conn_check(const Napi::CallbackInfo&, const Napi::Value& value) {
        omq->CONN_CHECK_INTERVAL = std::chrono::milliseconds{value.As<Napi::Number>().Int64Value()};
    }
    Napi::Value conn_check(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::Number>(info, omq->CONN_CHECK_INTERVAL.count());
    }
    void conn_heartbeat(const Napi::CallbackInfo&, const Napi::Value& value) {
        omq->CONN_HEARTBEAT = std::chrono::milliseconds{value.As<Napi::Number>().Int64Value()};
    }
    Napi::Value conn_heartbeat(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::Number>(info, omq->CONN_HEARTBEAT.count());
    }
    void conn_heartbeat_timeout(const Napi::CallbackInfo&, const Napi::Value& value) {
        omq->CONN_HEARTBEAT_TIMEOUT =
                std::chrono::milliseconds{value.As<Napi::Number>().Int64Value()};
    }
    Napi::Value conn_heartbeat_timeout(const Napi::CallbackInfo& info) {
        return napi_cast<Napi::Number>(info, omq->CONN_HEARTBEAT_TIMEOUT.count());
    }
    Napi::Value pubkey(const Napi::CallbackInfo& info) {
        return napi_copy_bytes(info.Env(), omq->get_pubkey());
    }
    Napi::Value privkey(const Napi::CallbackInfo& info) {
        return napi_copy_bytes(info.Env(), omq->get_privkey());
    }
    void start(const Napi::CallbackInfo&) {
        omq->start();
    }
    Napi::Value connect_remote(const Napi::CallbackInfo& info) {
        if (info.Length() < 1)
            throw std::invalid_argument{"An address is required"};
        NAddress* addr = NAddress::Unwrap(info[0].As<Napi::Object>());

        // FIXME: Need to make this async, so that the caller awaits a result that we set from
        // success/failure callbacks.
        auto on_success = [](oxenmq::ConnectionID) {
            std::cerr << "connect success!\n";
        };
        auto on_failure = [](oxenmq::ConnectionID, std::string_view reason) {
            std::cerr << "connection failed: " << reason << "\n";
        };
        auto c = omq->connect_remote(addr->get(), on_success, on_failure);

        // FIXME: what to return?
        return info.Env().Undefined();
        //return NConnectionID::wrap(info.Env(), c);
    }
};

}  // namespace noxenmq

// The wonderful NODE_API macros break if given a namespaced init function, so here we provide a
// lovely top-level function that wraps the namespaced one.
Napi::Object oxenmq_module_init(Napi::Env env, Napi::Object exports) {
    auto* ctors = new noxenmq::ctors{};
    noxenmq::NConnectionID::napi_init(env, exports, *ctors);
    noxenmq::NAddress::napi_init(env, exports, *ctors);
    noxenmq::NMessage::napi_init(env, exports, *ctors);
    noxenmq::NOxenMQ::napi_init(env, exports, *ctors);
    env.SetInstanceData(ctors);
    return exports;
}

NODE_API_MODULE(oxenmq, oxenmq_module_init)
