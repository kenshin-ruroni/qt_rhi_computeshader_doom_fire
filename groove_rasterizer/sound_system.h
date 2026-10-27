#pragma once

#include <QObject>
#include <QDir>
#include <QFile>
#include <QByteArray>
#include <QAudioFormat>
#include <QAudioOutput>
#include <QFileInfo>
#include <QDebug>
#include <QList>
#include <thread>
#include <unordered_map>
#include <string>
#include <string.h>
#include <algorithm>
#include <random>
#include <QAudioSink>
#include <QMediaDevices>
#include <QBuffer>
#include <QDebug>
#include <chrono>

#include "dr_wav.h"
#include "dr_flac.h"
#include "dr_mp3.h"

#pragma once
#include <QIODevice>
#include <QByteArray>

class AudioStream : public QIODevice {
    Q_OBJECT
public:
    explicit AudioStream(const QByteArray *data, QObject *parent = nullptr)
        : QIODevice(parent),  m_pos(0) {

        m_data.resize(data->size());
        memcpy( (void *)m_data.constData(), data->constData(),data->size() * sizeof(char));
        open(QIODevice::ReadOnly); // S'ouvre automatiquement en lecture
    }

    // Indique à Qt que c'est un flux de données linéaire (non-séquentiel comme un fichier standard)
    bool isSequential() const override { return false; }
    qint64 size() const override { return m_data.size(); }
    qint64 bytesAvailable() const override { return m_data.size() - m_pos; }
    bool seek(qint64 pos) override {
        if (pos >= 0 && pos <= m_data.size()) {
            m_pos = pos;
            return true;
        }
        return false;
    }

protected:

    qint64 readData(char *data, qint64 maxlen) override {
        if (m_pos >= m_data.size()) return 0; // Fin réelle du morceau

        qint64 len = qMin(maxlen, m_data.size() - m_pos);
        std::memcpy(data, m_data.constData() + m_pos, len);
        m_pos += len;
        return len; // Renvoie exactement ce que la carte son a demandé
    }

    qint64 writeData(const char *data, qint64 maxlen) override { return 0; } // Lecture seule

private:
    QByteArray m_data;
    qint64 m_pos;
};



class sound_system : public QObject
{
    Q_OBJECT
public:
    sound_system(){ start(); };

private:



    float heavyside_function(float t)
    {
        return static_cast<float>( t < 0.0f ) * 0. + (1.0f - static_cast<float>( t < 0.0f ) ) * 1.0f;
    }
    /*
      d = duration of fade in effect
    */
    void apply_logarithmic_fade_in(double fs, QByteArray *samples, size_t start_in_samples = 0, float d_seconds = 10.0f, float k = 2)
    {

        float dt = 1./fs;
        float t;
        size_t duration_in_samples = d_seconds * fs;
        float factor;
        for (size_t s = start_in_samples; s < start_in_samples + duration_in_samples;s += 2){
            t = static_cast<float>(s)*dt;
            factor = heavyside_function(d_seconds-t)*log(1.f + (std::numbers::e - 1.)* std::pow(t/d_seconds,k) ) + heavyside_function(t-d_seconds);
           // qDebug()<<t<<" "<<factor;
            (*samples)[s] = static_cast<int16_t>( (factor * static_cast<float>(static_cast<int16_t>((*samples)[s])) / 32768.0f) * 32767.0f);
        }

    }

    /*
      k is power of rise
      d = duration of fade in effect
    */
    void apply_logarithmic_fade_out(double fs,QByteArray *samples, float d_seconds = 10.0f, float k = 2)
    {

        float dt = 1./fs;
        float t;
        size_t duration_in_samples = d_seconds * fs;
        size_t start_in_samples = samples->size() - duration_in_samples;
        float factor;
        for (size_t s = start_in_samples; s < start_in_samples + duration_in_samples; s+= 2){
            t = static_cast<float>(s)*dt;
            factor = heavyside_function(d_seconds-t)*( 1. - log(1.f + (std::numbers::e - 1.)* std::pow(t/d_seconds,k) ) );
           // qDebug()<<t<<" "<<factor;
            (*samples)[s] = static_cast<int16_t>( (factor * static_cast<float>(static_cast<int16_t>((*samples)[s])) / 32768.0f) * 32767.0f);
        }

    }

     std::random_device rd;  // Source de chaos matérielle (seed)
     std::mt19937 gen = std::mt19937(rd()); // Moteur de rendu de nombres aléatoires (Mersenne Twister)

    std::uniform_int_distribution<size_t> distrib;


    QString getExtension(const QString &filePath) {
        QFileInfo fileInfo(filePath);
        return fileInfo.suffix();
    }

    QString get_filename(const QString &filePath) {
        QFileInfo fileInfo(filePath);
        return fileInfo.fileName();
    }


    // Structure contenant les informations du morceau décodé en mémoire
    struct audio_track {

        size_t id;
        size_t hash;
        int channels = 0;          // Nombre de canaux (ex: 2 pour Stéréo)
        int sampleRate = 0;        // Fréquence d'échantillonnage (ex: 44100 Hz)
        qint64 durationMs = 0;     // Durée totale du morceau en millisecondes

        // Encodage d'origine ou format PCM actuel
        // QAudioFormat::SampleFormat (ex: QAudioFormat::Int16, QAudioFormat::Float)
        QAudioFormat::SampleFormat sampleFormat = QAudioFormat::Unknown;
        QByteArray pcmData;        // Le buffer de données PCM brutes natif de Qt

    };

    std::unordered_map< size_t , audio_track > audio_tracks;

    alignas(64) std::atomic<bool> initialized = false;

     alignas(64) std::atomic<bool> stop_requested = false;

    alignas(64) std::atomic<bool> stopped = false;

    alignas(64) size_t current_track_id;
    alignas(64) std::atomic<size_t> all_ids = 0;

    inline void start()
    {

        std::thread( [this]()->void{

            load_all_resources();
            if ( audio_tracks.empty())
            {
                qDebug()<<"aucune piste audio trouvee. sound system is shutting down.";
                return;
            }
            while (!stopped.load() )
            {
                load_and_play();
            }


        }).detach();

    }

    inline void stop_playback()
    {
        emit this->audioSink->stateChanged(QAudio::State::StoppedState);

    }


    // Fonction qui lance la lecture du PCM en mémoire
    // Note : Le QByteArray doit contenir du PCM brut (ex: décodé par dr_libs) et NON du MP3 compressé.

    QAudioSink *audioSink   = nullptr;
    AudioStream *audioDevice = nullptr;

    audio_track *current_audio_track = nullptr;



    inline void change_audio_sink_state(QAudio::State newState)
    {
        qDebug()<<" le thread de lecture change d'etat "<<newState;
        if ( newState == QAudio::StoppedState )
        {
            this->audioSink->stop();
            this->audioDevice->close();
            QObject::disconnect(this->audioSink, &QAudioSink::stateChanged,this,&sound_system::change_audio_sink_state);
            // Destruction propre des pointeurs pour éviter les fuites mémoire
            delete this->audioSink;
            delete this->audioDevice;
            this->current_audio_track = nullptr;
            qDebug() << "Lecture terminée, mémoire libérée.";
        }

    };

    void load_and_play()
    {
        if ( current_audio_track == nullptr)
        {
            current_track_id = distrib(gen);
                auto track_id = std::find_if(
                    audio_tracks.begin(),audio_tracks.end(),[this](const std::pair<size_t,audio_track> &t)->bool
                    {
                        return t.second.id == this->current_track_id;
                    }
                );

                if (track_id != audio_tracks.end())
                {
                    current_audio_track = &track_id->second;
                }
                else
                {
                    auto first_track =    audio_tracks.begin();++first_track;
                    current_audio_track = &first_track->second;
                }
        }
        else{
            return;
        }


        QAudioFormat format;
        format.setSampleRate(current_audio_track->sampleRate);
        format.setChannelCount(current_audio_track->channels);
        format.setSampleFormat(QAudioFormat::Int16);

        QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
        if (defaultDevice.isNull()) {
            qWarning() << "Aucun périphérique audio de sortie n'a été détecté sur le système !";
            return;
        }


        // 2. Initialisation du moteur de rendu dans CE thread
        QAudioSink* audioSink = new QAudioSink(defaultDevice, format);
        qDebug() << "Périphérique par défaut détecté :" << defaultDevice.description();
        // 3. Démarrage en mode PUSH pour récupérer le canal d'écriture matériel direct
        QIODevice* outputDevice = audioSink->start();
        if (!outputDevice) {
            qWarning() << "Impossible d'ouvrir le canal audio.";
            delete audioSink;
            current_audio_track = nullptr;
            return;
        }

        // 4. Copie locale des données pour isolation
        QByteArray pcmData = current_audio_track->pcmData;
        qint64 totalBytes = pcmData.size();
        qint64 bytesWritten = 0;

        qInfo() << "Démarrage de la boucle de streaming synchrone dans le thread dédié...";

        qDebug() << "Lecture du morceau démarrée en tâche de fond...";

        // 5. BOUCLE SYNCHRONE ET BLOQUANTE (Valide uniquement parce que vous êtes dans un thread séparé !)
        while (bytesWritten < totalBytes && audioSink->state() != QAudio::StoppedState) {

            // On interroge l'espace disponible dans le tampon de la carte son
            int bytesFree = audioSink->bytesFree();

            if (bytesFree > 0) {
                // On calcule la taille du bloc à pousser (4096 échantillons ou moins)
                qint64 chunkSize = qMin(static_cast<qint64>(bytesFree), totalBytes - bytesWritten);

                if (chunkSize > 0) {
                    qint64 written = outputDevice->write(pcmData.constData() + bytesWritten, chunkSize);
                    if (written > 0) {
                        bytesWritten += written;
                    } else if (written < 0) {
                        qWarning() << "Erreur d'écriture sur le périphérique audio.";
                        break;
                    }
                }
            }

            // Crucial : On endort le thread pendant 10ms pour laisser la carte son
            // vider son tampon et éviter de saturer le CPU à 100%
            std::this_thread::sleep_for(std::chrono::milliseconds(10) );
        }

        // 6. Nettoyage synchrone une fois le morceau entièrement joué
        qInfo() << "Piste entièrement transmise à la carte son. Fin du thread audio.";
        audioSink->stop();
        delete audioSink;
        current_audio_track = nullptr;
    }



    bool decode_wav(const QString &filePath, audio_track *outTrack)
    {
        drwav wav;

        // 1. Initialiser le décodeur à partir du chemin du fichier
        if (!drwav_init_file(&wav, filePath.toLocal8Bit().constData(), nullptr)) {
            qWarning() << "dr_wav : Impossible d'ouvrir le fichier :" << filePath;
            return false;
        }

        // 2. Extraire les métadonnées du format d'origine
        outTrack->channels = wav.channels;
        outTrack->sampleRate = wav.sampleRate;
        outTrack->sampleFormat = QAudioFormat::Int16; // On force le décodage en Int16 pour la carte son

        // 3. Calculer la taille totale nécessaire en octets (bytes)
        // 1 frame = (nombre de canaux) * (taille d'un échantillon s16, soit 2 octets) [1]
        size_t bytesPerFrame = wav.channels * sizeof(drwav_int16);
            drwav_uint64 totalFrames = wav.totalPCMFrameCount;
            drwav_uint64 totalBytes = totalFrames * bytesPerFrame;

        // 4. Allouer directement la mémoire dans le QByteArray
        outTrack->pcmData.resize(totalBytes);

        // 5. Décoder et copier les trames directement dans la mémoire de Qt [1]
        drwav_uint64 framesRead = drwav_read_pcm_frames_s16( &wav,
                                                            totalFrames,
                                                            reinterpret_cast<drwav_int16*>(outTrack->pcmData.data())
                                                            );

        // Ajuster la taille réelle si le nombre de trames lues est différent
        if (framesRead < totalFrames) {
            outTrack->pcmData.resize(framesRead * bytesPerFrame);
        }

        // 6. Calculer la durée totale du morceau en millisecondes
        if (outTrack->sampleRate > 0) {
            outTrack->durationMs = (framesRead * 1000) / outTrack->sampleRate;
        }

        // Libération des ressources de dr_wav [1]
        drwav_uninit(&wav);
            return true;
    }

    bool decode_mp3(const QString &filePath, audio_track *outTrack)
    {
        drmp3 mp3;

        // Initialisation depuis le fichier (dr_mp3 gère les chemins natifs standard)
        // Note: Si vous lisez depuis les ressources intégrées Qt ":/", utilisez drmp3_init_memory à la place.
        if (!drmp3_init_file(&mp3, filePath.toLocal8Bit().constData(), nullptr)) {
            qWarning() << "dr_mp3 : Impossible d'ouvrir ou d'initialiser le fichier :" << filePath;
            return false;
        }

        // 1. Récupération des métadonnées du format d'origine
        outTrack->channels = mp3.channels;
        outTrack->sampleRate = mp3.sampleRate;
        outTrack->sampleFormat = QAudioFormat::Int16; // dr_mp3 décode ici en Int16 (s16)

        // 2. Calculer la taille totale nécessaire en octets (bytes)
        // Une frame = (nombre de canaux) * (taille d'un échantillon s16, soit 2 octets)
        size_t bytesPerFrame = mp3.channels * sizeof(drmp3_int16);
        drmp3_uint64 totalFrames = drmp3_get_pcm_frame_count(&mp3);
        drmp3_uint64 totalBytes = totalFrames * bytesPerFrame;

        // 3. Allouer directement la mémoire dans le QByteArray de notre structure
        outTrack->pcmData.resize(totalBytes);

        // 4. Décoder les frames directement dans le buffer du QByteArray
        // On passe le pointeur brut vers les données internes de Qt via .data()
        drmp3_uint64 framesRead = drmp3_read_pcm_frames_s16(
            &mp3,
            totalFrames,
            reinterpret_cast<drmp3_int16*>(outTrack->pcmData.data())
            );

        // Ajuster la taille finale du QByteArray si le nombre de frames lues diffère
        if (framesRead < totalFrames) {
            outTrack->pcmData.resize(framesRead * bytesPerFrame);
        }

        // 5. Calculer la durée totale du morceau en millisecondes
        if (outTrack->sampleRate > 0) {
            outTrack->durationMs = (framesRead * 1000) / outTrack->sampleRate;
        }

        qDebug() << "Décodage réussi !"
                 << "Frames lues :" << framesRead
                 << "Taille finale :" << outTrack->pcmData.size() << "octets"
                 << "Durée théorique :" << outTrack->durationMs << "ms";

        // Libération des ressources de dr_mp3
        drmp3_uninit(&mp3);
        return true;
    }

    bool decode_flac(const QString &filePath, audio_track *outTrack)
    {
        // 1. Initialiser le décodeur depuis le chemin du fichier
        drflac* pFlac = drflac_open_file(filePath.toLocal8Bit().constData(), nullptr);
        if (!pFlac) {
            qWarning() << "dr_flac : Impossible d'ouvrir le fichier :" << filePath;
            return false;
        }

        // 2. Extraire les métadonnées de la structure drflac
        outTrack->channels = pFlac->channels;
        outTrack->sampleRate = pFlac->sampleRate;
        outTrack->sampleFormat = QAudioFormat::Int16; // On choisit de forcer le décodage en Int16

        // 3. Calculer la taille totale en octets (bytes)
        // 1 frame = (canaux) * (taille d'un int16_t, soit 2 octets)
        size_t bytesPerFrame = pFlac->channels * sizeof(drflac_int16);
        drflac_uint64 totalFrames = pFlac->totalPCMFrameCount;
        drflac_uint64 totalBytes = totalFrames * bytesPerFrame;

        // 4. Allouer la mémoire directement dans le QByteArray
        outTrack->pcmData.resize(totalBytes);

        // 5. Lire et convertir à la volée les trames en s16 (16 bits) directement dans le buffer Qt
        drflac_uint64 framesRead = drflac_read_pcm_frames_s16(
            pFlac,
            totalFrames,
            reinterpret_cast<drflac_int16*>(outTrack->pcmData.data())
            );

        // Ajuster si le nombre de trames lues est différent
        if (framesRead < totalFrames) {
            outTrack->pcmData.resize(framesRead * bytesPerFrame);
        }

        // 6. Calcul de la durée en millisecondes
        if (outTrack->sampleRate > 0) {
            outTrack->durationMs = (framesRead * 1000) / outTrack->sampleRate;
        }

        // Libération des ressources internes du décodeur
        drflac_close(pFlac);
        return true;
    }


    inline void load_all_resources()
    {

        const QString &prefixPath = QString("../../music");

        QDir directory(prefixPath);
        if (!directory.exists()) {
            qWarning() << "Le préfixe de ressource n'existe pas :" << prefixPath;
            return;
        }

        // 2. Filtrer pour ne récupérer que les fichiers (pas les sous-dossiers)
        QStringList files = directory.entryList(QDir::Files);

        initialized.store(false);

        bool ok = false;

        // 3. Boucler sur chaque fichier trouvé
        for (const QString &fileName : files) {
            // Reconstruire le chemin absolu de la ressource (ex: ":/audio/sound.mp3")
            QString absoluteResourcePath = directory.absoluteFilePath(fileName);

            QFile file(absoluteResourcePath);

            size_t hash = std::hash<std::string>{}(absoluteResourcePath.toStdString());

            auto test_track = std::find_if(  audio_tracks.begin(), audio_tracks.end(), [&,hash](const std::pair<size_t,audio_track> p)->bool
            {
                return p.second.hash == hash ;
            });

            if (test_track != audio_tracks.end() )
            {
                qDebug() << "Ressource " << absoluteResourcePath  << "existe deja.";
                continue;
            }

            if (file.open(QIODevice::ReadOnly))
            {

                QString ext = getExtension(absoluteResourcePath);

                audio_track new_track;
                if (ext == "wav")
                {
                   ok =  decode_wav(absoluteResourcePath.toStdString().c_str(),&new_track);
                }
                else if (ext == "flac")
                {
                   ok =  decode_flac(absoluteResourcePath.toStdString().c_str(),&new_track);

                }
                else if (ext == "mp3")
                {
                   ok =  decode_mp3(absoluteResourcePath.toStdString().c_str(),&new_track);
                }

                file.close();

                if ( ok )
                {
                    apply_logarithmic_fade_in(new_track.sampleRate,&new_track.pcmData);
                    apply_logarithmic_fade_out(new_track.sampleRate,&new_track.pcmData);
                    qDebug() << "Ressource chargée :" << absoluteResourcePath
                             << "Taille :" << new_track.pcmData.size() << "octets";
                    new_track.id = all_ids++;
                    new_track.hash = hash;
                    audio_tracks.emplace(new_track.id, std::move(new_track));
                }

            } else
            {
                qWarning() << "Impossible d'ouvrir la ressource :" << absoluteResourcePath;
            }
        }
        distrib = std::uniform_int_distribution<size_t>(0, all_ids.load() ); // Distribution uniforme [0, max]
        initialized.store(true);

        current_track_id = distrib(gen);

    }
};

