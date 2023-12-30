//
// Created by Laky64 on 16/08/2023.
//

#include "peer_connection_factory.hpp"
#include <rtc_base/ssl_adapter.h>
#include <api/create_peerconnection_factory.h>
#include <api/rtc_event_log/rtc_event_log_factory.h>
#include <api/task_queue/default_task_queue_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include "peer_connection_factory_with_context.hpp"
#include "../../video_factory/video_factory_config.hpp"

namespace wrtc {
    std::mutex PeerConnectionFactory::_mutex{};
    int PeerConnectionFactory::_references = 0;
    rtc::scoped_refptr<PeerConnectionFactory> PeerConnectionFactory::_default = nullptr;

    PeerConnectionFactory::PeerConnectionFactory() {
        network_thread_ = rtc::Thread::CreateWithSocketServer();
        network_thread_->Start();
        worker_thread_ = rtc::Thread::Create();
        worker_thread_->Start();
        signaling_thread_ = rtc::Thread::Create();
        signaling_thread_->Start();

        webrtc::PeerConnectionFactoryDependencies dependencies;
        dependencies.network_thread = network_thread_.get();
        dependencies.worker_thread = worker_thread_.get();
        dependencies.signaling_thread = signaling_thread_.get();
        dependencies.task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
        dependencies.call_factory = webrtc::CreateCallFactory();
        dependencies.event_log_factory =
                absl::make_unique<webrtc::RtcEventLogFactory>(
                        dependencies.task_queue_factory.get());
        cricket::MediaEngineDependencies media_dependencies;
        media_dependencies.task_queue_factory = dependencies.task_queue_factory.get();
        media_dependencies.adm = worker_thread_->BlockingCall([&] {
            if (!_audioDeviceModule)
                _audioDeviceModule = webrtc::AudioDeviceModule::Create(webrtc::AudioDeviceModule::kDummyAudio, dependencies.task_queue_factory.get());
            return _audioDeviceModule;
        });
        auto config = VideoFactoryConfig();
        media_dependencies.audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
        media_dependencies.audio_decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
        media_dependencies.video_encoder_factory = config.CreateVideoEncoderFactory();
        media_dependencies.video_decoder_factory = config.CreateVideoDecoderFactory();
        media_dependencies.audio_mixer = nullptr;
        media_dependencies.audio_processing = webrtc::AudioProcessingBuilder().Create();
        dependencies.media_engine = CreateMediaEngine(std::move(media_dependencies));
        if (!_factory) {
            _factory = PeerConnectionFactoryWithContext::Create(std::move(dependencies), connection_context_);
        }
        webrtc::PeerConnectionFactoryInterface::Options options;
        options.disable_encryption = false;
        options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
        options.crypto_options.srtp.enable_gcm_crypto_suites = true;
        _factory->SetOptions(options);
    }

    PeerConnectionFactory::~PeerConnectionFactory() {
        _factory = nullptr;

        if (_audioDeviceModule) {
            worker_thread_->BlockingCall([this]
            {
                if (_audioDeviceModule)
                    _audioDeviceModule = nullptr;
            });
        }

        worker_thread_->Stop();
        signaling_thread_->Stop();
        network_thread_->Stop();
    }

    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> PeerConnectionFactory::factory() {
        return _factory;
    }

    rtc::scoped_refptr<PeerConnectionFactory> PeerConnectionFactory::GetOrCreateDefault() {
        _mutex.lock();
        _references++;
        if (_references == 1) {
            rtc::InitializeSSL();
            _default = rtc::scoped_refptr<PeerConnectionFactory>(new rtc::RefCountedObject<PeerConnectionFactory>());
        }
        _mutex.unlock();
        return _default;
    }

    void PeerConnectionFactory::UnRef() {
        _mutex.lock();
        _references--;
        if (!_references) {
            rtc::CleanupSSL();
            _default = nullptr;
        }
        _mutex.unlock();
    }
} // wrtc