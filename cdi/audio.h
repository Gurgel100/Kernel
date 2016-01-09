/*
 * Copyright (c) 2010 Max Reitz
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/projects/COPYING.WTFPL for more details.
 */

/**
 * \german
 * Treiber für Soundkarten
 * \endgerman
 * \english
 * Sound card drivers
 * \endenglish
 * \defgroup audio
 */
/*\@{*/

#ifndef _CDI_AUDIO_H_
#define _CDI_AUDIO_H_

#include <stddef.h>
#include <stdint.h>

#include "cdi.h"
#include "cdi/lists.h"
#include "cdi/mem.h"

/**
 * \german
 * Mögliche Sampleformate (also Integer vorzeichenbehaftet/vorzeichenlos,
 * Gleitkommazahlen, Größe eines Samples, ...).
 * Wenn nicht anders gekennzeichnet, wird Little Endian verwendet.
 * \endgerman
 * \english
 * Possible sample formats (i.e., signed/unsigned integer, floating point
 * numbers, one sample's size, ...).
 * Little endian is used, unless marked otherwise.
 * \endenglish
 */
typedef enum {
    // 16 bit signed integer
    CDI_AUDIO_16SI = 0,
    // 8 bit signed integer
    CDI_AUDIO_8SI,
    // 32 bit signed integer
    CDI_AUDIO_32SI
} cdi_audio_sample_format_t;

/**
 * \german
 * Abspielposition eines Kanals. Enthält den Index des aktuellen Puffers und
 * des aktuell abgespielten Frames.
 * \endgerman
 * \english
 * A stream's playback position. Contains the index of both the current buffer
 * and the frame being played.
 * \endenglish
 */
typedef struct cdi_audio_position {
    size_t buffer;
    size_t frame;
} cdi_audio_position_t;

/**
 * \german
 * Status eines Kanals.
 * \endgerman
 * \english
 * A stream's status.
 * \endenglish
 */
typedef enum {
    /**
     * \german
     * Angehalten
     * \endgerman
     * \english
     * Stopped
     * \endenglish
     */
    CDI_AUDIO_STOP = 0,
    /**
     * \german
     * Entweder abspielen oder aufnehmen, je nach Typ.
     * \endgerman
     * \english
     * According to the device type either playing or recording.
     * \endenglish
     */
    CDI_AUDIO_PLAY = 1
} cdi_audio_status_t;

/**
 * \german
 * Beschreibt ein CDI.audio-Gerät, also Tonabspiel- oder Aufnahmehardware.
 * Bietet eine Soundkarte beide Funktionen an, so muss sie in zwei CDI.audio-
 * Geräte aufgeteilt werden.
 * \endgerman
 * \english
 * Describes a CDI.audio device, i.e. a playback or recording device. If a
 * sound card provides both functions, the driver must register one CDI.audio
 * device per function.
 * \endenglish
 */
struct cdi_audio_device {
    struct cdi_device dev;

    /**
     * \german
     * Ist dieser Wert ungleich 0, so handelt es sich um ein Aufnahmegerät,
     * es kann nur gelesen werden. Ist er 0, dann können nur Töne abgespielt
     * werden.
     * \endgerman
     * \english
     * If this value is not 0, this device is a recording device, thus one can
     * only read data. Otherwise, only play back is possible.
     * \endenglish
     */
    int record;

    /**
     * \german
     * Abspielgeräte können mehrere logische Kanäle anbieten, die vom Gerät
     * zusammengemixt werden. Ist dem nicht der Fall oder handelt es sich um
     * ein Aufnahmegerät, dann ist die Größe dieser Liste genau 1. Sie enthält
     * Elemente des Typs struct cdi_audio_stream.
     * \endgerman
     * \english
     * Play back devices may provide several logical streams which are mixed by
     * the device. If this is either not true or it is a recording device, the
     * list's size is exactly 1. It contains elements of type
     * struct cdi_audio_stream.
     * \endenglish
     */
    cdi_list_t streams;
};

/**
 * \german
 * Beschreibt einen Kanal eines CDI.audio-Geräts.
 * Jeder Kanal besteht aus einer Reihe von Puffern mit abzuspielenden Samples,
 * die hintereinander und in einer Schleife abgespielt werden.
 * \endgerman
 * \english
 * Describes a CDI.audio device's stream.
 * Each stream consists of a set of buffers containing samples being played
 * consecutively and as a loop.
 * \endenglish
 */
struct cdi_audio_stream {
    /**
     * \german
     * Gerät, zu dem dieser Kanal gehört.
     * \endgerman
     * \english
     * Device to which this stream belongs.
     * \endenglish
     */
    struct cdi_audio_device* device;

    /**
     * \german
     * Gibt die Anzahl der verfügbaren Datenpuffer an.
     * \endgerman
     * \english
     * Contains the number of available data buffers.
     * \endenglish
     */
    size_t num_buffers;

    /**
     * \german
     * Gibt die Anzahl von Samples pro Datenpuffer an.
     * \endgerman
     * \english
     * Contains the number of samples per buffer.
     * \endenglish
     */
    size_t buffer_size;

    /**
     * \german
     * Bei 0 kann die Samplerate eingestellt werden. Ansonsten kann sie nicht
     * verändert werden und hat einen festen (hier angegebenen) Wert (in Hz).
     * \endgerman
     * \english
     * If 0, the sample rate is adjustable. Otherwise, it isn't and is set to
     * a fixed value in Hz (which is given here).
     * \endenglish
     */
    int fixed_sample_rate;

    /**
     * \german
     * Format der Samples in den Puffern.
     * \endgerman
     * \english
     * Format of the samples contained in the buffers.
     * \endenglish
     */
    cdi_audio_sample_format_t sample_format;
};

struct cdi_audio_driver {
    struct cdi_driver drv;

    /**
     * \german
     *
     * Überträgt Daten von oder zu einem Puffer eines Geräts (bei
     * Aufnahmegeräten wird empfangen, sonst gesendet). Es werden so viele
     * Bytes übertragen, wie in den Puffer passen (abhängig von Sampleformat und
     * Anzahl der Samples pro Puffer).
     *
     * @param stream Kanal, der angesprochen werden soll.
     * @param buffer Der dort zu verwendende Puffer.
     * @param memory Speicher, mit dem die Daten ausgetauscht werden sollen.
     * @param offset Offset im Puffer (in Samples), an den die Daten kopiert
     *               werden (es wird nur so viel kopiert, bis das Ende des
     *               Puffers erreicht wird), wird bei Aufnahmegeräten ignoriert.
     *
     * @return 0 bei Erfolg, -1 im Fehlerfall.
     *
     * \endgerman
     * \english
     *
     * Sends or receives data to/from a device's buffer (receives from recording
     * devices, sends otherwise). The amount of data transmitted equals the
     * buffer's size (depends on the sample format and the number of samples per
     * buffer) reduced by the offset.
     *
     * @param stream Stream to be used.
     * @param buffer Buffer to be used.
     * @param memory Memory buffer being either source or destination.
     * @param offset The content from memory is copied to the buffer respecting
     *               this offset (in samples) in the buffer. Thus, the data is
     *               copied to this offset until the end of the buffer has been
     *               reached (recording devices ignore this).
     *
     * @return 0 on success, -1 on error.
     *
     * \endenglish
     */
    int (*transfer_data)(struct cdi_audio_stream* stream, size_t buffer,
        struct cdi_mem_area* memory, size_t offset);

    /**
     * \german
     *
     * Hält ein CDI.audio-Gerät an oder lässt es aufnehmen bzw. abspielen. Wird
     * es angehalten, so wird die Position automatisch auf den Anfang des ersten
     * Puffers gesetzt.
     *
     * @param device Das Gerät
     * @param status Zu setzender Status.
     *
     * @return Tatsächlicher neuer Status.
     *
     * \endgerman
     * \english
     *
     * Stops a CDI.audio device or makes it record or play, respectively. If it
     * is stopped, its position is automatically set to the beginning of the
     * first buffer.
     *
     * @param device The device
     * @param status Status to be set.
     *
     * @return Actual new status.
     *
     * \endenglish
     */
    cdi_audio_status_t (*change_device_status)(struct cdi_audio_device* device,
        cdi_audio_status_t status);

    /**
     * \german
     *
     * Setzt die Lautstärke eines Kanals.
     *
     * @param volume 0x00 ist stumm, 0xFF ist volle Lautstärke.
     *
     * \endgerman
     * \english
     *
     * Sets a stream's volume.
     *
     * @param volume 0x00 is mute, 0xFF is full volume.
     *
     * \endenglish
     */
    void (*set_volume)(struct cdi_audio_stream* stream, uint8_t volume);

    /**
     * \german
     *
     * Verändert die Samplerate eines Kanals.
     *
     * @param stream Anzupassender Kanal.
     * @param sample_rate Neue Samplerate, ist der Wert nichtpositiv, dann wird
     *                    die aktuelle Samplerate nicht verändert.
     *
     * @return Gibt die tatsächliche neue Samplerate zurück (kann vom Parameter
     *         abweichen und wird es bei nichtpositivem auch).
     *
     * \endgerman
     * \english
     *
     * Changes a stream's sample rate.
     *
     * @param stream Stream to be adjusted.
     * @param sample_rate New sample rate, if nonpositive, the current sample
     *                    rate won't be changed.
     *
     * @return Returns the actual new sample rate (may differ from the parameter
     *         and will, if the latter was nonpositive).
     *
     * \endenglish
     */
    int (*set_sample_rate)(struct cdi_audio_stream* stream, int sample_rate);

    /**
     * \german
     *
     * Gibt die aktuelle Abspiel-/Aufnahmeposition des Kanals zurück.
     *
     * @param stream Abzufragender Kanal.
     * @param position Zeiger zu einer Struktur, die den Index des aktuellen
     *                 Puffers und des derzeit abgespielten Frames empfängt.
     *
     * \endgerman
     * \english
     *
     * Returns the stream's current playback/recording position.
     *
     * @param stream Stream to be queried.
     * @param position Pointer to a structure receiving the index of both the
     *                 current buffer and the frame played right now.
     *
     * \endenglish
     */
    void (*get_position)(struct cdi_audio_stream* stream,
        cdi_audio_position_t* position);

    /**
     * \german
     *
     * Verändert die Anzahl der von einem CDI.audio-Gerät verwendeten Channel
     * (da dies die Größe der Frames verändert, kann auch die Anzahl der
     * tatsächlich verwendeten Samples in einem Puffer verändert werden:
     * Beträgt diese Maximalzahl z. B. 0xFFFF, dann können bei zwei Channeln
     * nur 0xFFFE Samples verwendet werden (da es keine halben Frames gibt)).
     *
     * @param dev CDI.audio-Gerät
     * @param channels Anzahl der zu verwendenden Channels (bei ungültigem Wert
     *                 wird am Gerät nichts verändert).
     *
     * @return Jetzt tatsächlich benutzte Channelanzahl.
     *
     * \endgerman
     * \english
     *
     * Changes the number of channels used by a CDI.audio device (this changes
     * the frames' size, hence the number of actually used samples per buffer
     * may be changed, too: If this size equals e.g. 0xFFFF, only 0xFFFE samples
     * are used when working with two channels (because there is no half of a
     * frame)).
     *
     * @param dev CDI.audio device
     * @param channels Number of channels to be used (if invalid, nothing will
     *                 be changed).
     *
     * @return Actually used number of channels now.
     *
     * \endenglish
     */
    int (*set_number_of_channels)(struct cdi_audio_device* dev, int channels);
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * \german
 *
 * Wird vom Treiber aufgerufen, wenn die Bearbeitung eines Puffers abgeschlossen
 * wurde (komplett abgespielt oder aufgenommen). Darf auch aus einer ISR heraus
 * aufgerufen werden.
 *
 * @param stream Kanal, den es betrifft.
 * @param buffer Pufferindex, der bearbeitet wurde.
 *
 * \endgerman
 * \english
 *
 * Is called by a driver when having completed a buffer (i.e., completely played
 * or recorded). May be called by an ISR.
 *
 * @param stream Stream being concerned.
 * @param buffer Index of the completed buffer.
 *
 * \endenglish
 */
void cdi_audio_buffer_completed(struct cdi_audio_stream* stream, size_t buffer);

#ifdef __cplusplus
}; // extern "C"
#endif

#endif

/*\@}*/

