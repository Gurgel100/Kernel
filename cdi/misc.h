/*
 * Copyright (c) 2007 Kevin Wolf
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it 
 * and/or modify it under the terms of the Do What The Fuck You Want 
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/projects/COPYING.WTFPL for more details.
 */  

#ifndef _CDI_MISC_H_
#define _CDI_MISC_H_

#include <stdint.h>

#include <cdi.h>

#define CDI_GLUE(x,y) x ## y
#define CDI_BUILD_ASSERT(cnt) CDI_GLUE(__cdi_build_assert, cnt)
#define CDI_BUILD_BUG_ON(x) \
    struct CDI_BUILD_ASSERT(__COUNTER__) { int assertion[(x) ? -1 : 1];  };


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Registiert einen neuen IRQ-Handler.
 *
 * @param irq Nummer des zu reservierenden IRQ
 * @param handler Handlerfunktion
 * @param device Geraet, das dem Handler als Parameter uebergeben werden soll
 */
void cdi_register_irq(uint8_t irq, void (*handler)(struct cdi_device*), 
    struct cdi_device* device);

/**
 * Setzt den IRQ-Zaehler fuer cdi_wait_irq zurueck.
 *
 * @param irq Nummer des IRQ
 *
 * @return 0 bei Erfolg, -1 im Fehlerfall
 */
int cdi_reset_wait_irq(uint8_t irq);

/**
 * Wartet bis der IRQ aufgerufen wurde. Der interne Zähler muss zuerst mit
 * cdi_reset_wait_irq zurückgesetzt werden. Damit auch die IRQs abgefangen
 * werden können, die kurz vor dem Aufruf von dieser Funktion aufgerufen
 * werden, sieht der korrekte Ablauf wie folgt aus:
 *
 * -# cdi_reset_wait_irq
 * -# Hardware ansprechen und Aktionen ausführen, die schließlich den IRQ
 *    auslösen
 * -# cdi_wait_irq
 *
 * Der entsprechende IRQ muss zuvor mit cdi_register_irq registriert worden
 * sein. Der registrierte Handler wird ausgeführt, bevor diese Funktion
 * erfolgreich zurückkehrt.
 *
 * @param irq       Nummer des IRQ auf den gewartet werden soll
 * @param timeout   Anzahl der Millisekunden, die maximal gewartet werden sollen
 *
 * @return 0 wenn der irq aufgerufen wurde, -1 sonst.
 */
int cdi_wait_irq(uint8_t irq, uint32_t timeout);

/**
 * Reserviert IO-Ports
 *
 * @return 0 wenn die Ports erfolgreich reserviert wurden, -1 sonst.
 */
int cdi_ioports_alloc(uint16_t start, uint16_t count);

/**
 * Gibt reservierte IO-Ports frei
 *
 * @return 0 wenn die Ports erfolgreich freigegeben wurden, -1 sonst.
 */
int cdi_ioports_free(uint16_t start, uint16_t count);

/**
 * Unterbricht die Ausfuehrung fuer mehrere Millisekunden
 */
void cdi_sleep_ms(uint32_t ms);

/**
 * \german
 * Gibt die Anzahl der vergangenen Millisekunden seit einem
 * implementationsspezifischen Zeitpunkt zurück, mindestens aber seit dem ersten
 * Aufruf dieser Funktion. Die Auflösung ist ebenfalls
 * implementationsspezifisch. Der Rückgabewert muss monoton wachsen (bis er
 * wegen der Begrenzung des Rückgabetyps überläuft).
 * \endgerman
 * \english
 * Returns the number of milliseconds elapsed since an implementation specific
 * point in time, not later than the first call of this function. The resolution
 * too is implementation specific. The return value must grow monotonically
 * (until it overflows due to the return type's limit).
 * \endenglish
 * \thuringiansaxonian
 * Dud zurückgehm, wie lange (in Millisegundn) ä imblämändazschjounsspäzifschr
 * Zeitbunkt nu schon her is. Diesr Zeitbunkt muss oor vorm erstn Uffruf diesr
 * Fungzschjoun lieschn. De Ufflösung is ooch imblämändazschjounsspäzifsch. Dr
 * Rückgabewert muss monodohn wachsn (bissr wäschn dä Begrenzung vondn
 * Rückgabedühp übrloofn dud).
 * \endthuringiansaxonian
 * \french
 * Rend le nombre des millisecondes qui ont passé depuis un instant défini par
 * implémentation, au plus tard depuis le premier appel de cette fonction. La
 * résolution aussi est défini par implémentation. La valeur rendue doit grandir
 * monotonement (jusqu'elle dépasse par la limitation du type rendu).
 * \endfrench
 * \japanese
 * 実現の特有な時点からミリ秒の総数を返す。その時点はこのファンクションを
 * 呼び出すことの後だめ。分解能も実現の特有。返される数値は単調に弥増す
 * （返されるタイプの限界因ってオーバーフローまで）。
 * \endjapanese
 */
uint64_t cdi_elapsed_ms(void);

/**
 * \german
 * Wartet, bis die Bedingung @cond erfüllt ist oder mehr als @timeout
 * Millisekunden vergangen sind.
 * \endgerman
 * \english
 * Waits until the condition @cond yields true or more than @timeout
 * milliseconds have passed.
 * \endenglish
 * \thuringiansaxonian
 * Dud wortn, bisde Bedingung @cond erfüllt is oodr über @timeout Millisekundn
 * vorbei sin.
 * \endthuringiansaxonian
 * \french
 * Attend que la condition @cond est satisfaite ou plus que @timeout
 * millisecondes sont passées.
 * \endgerman
 * \japanese
 * 条件@condの該当までか@timeoutミリ秒間待つ。
 * \endjapanese
 */
#define CDI_CONDITION_WAIT(cond, timeout) \
    do { \
        uint64_t start = cdi_elapsed_ms(); \
        while (!(cond) && cdi_elapsed_ms() - start < timeout); \
    } while (0)

/**
 * \german
 * CDI_CONDITION_WAIT() mit cdi_sleep_ms(@sleep) zwischen jeder Auswertung.
 * \endgerman
 * \english
 * CDI_CONDITION_WAIT() with cdi_sleep_ms(@sleep) between each evaluation.
 * \endenglish
 * \thuringiansaxonian
 * CDI_CONDITION_WAIT() mittä cdi_sleep_ms(@sleep) zwischn jädr Auswärdungn.
 * \endthuringiansaxonian
 * \french
 * CDI_CONDITION_WAIT() avec cdi_sleep_ms(@sleep) entre toutes les évaluations.
 * \endfrench
 * \japanese
 * 全ての検査の間にcdi_sleep_ms(@sleep)を呼び出すCDI_CONDITION_WAIT()。
 * \endjapanese
 */
#define CDI_CONDITION_WAIT_SLEEP(cond, timeout, sleep) \
    do { \
        uint64_t start = cdi_elapsed_ms(); \
        while (!(cond) && cdi_elapsed_ms() - start < timeout) { \
            cdi_sleep_ms(sleep); \
        } \
    } while (0)

/**
 * \german
 * Castet @object (muss ein Pointer sein) zu einem Pointer auf ein Objekt des
 * Typs @to, welcher eine Unterklasse vom Typ von @object sein muss. @field ist
 * das Feld von @to, das @object enthält.
 *
 * Beispiel:
 * struct cdi_driver* driver;
 * struct cdi_storage_driver* storage_driver =
 *     CDI_UPCAST(driver, struct cdi_storage_driver, drv);
 * \endgerman
 * \english
 * Casts @object (must be a pointer) to a pointer to an object of type @to which
 * must be a subclass of @object's type. @field is the entry in @to which
 * contains @object.
 *
 * Example:
 * struct cdi_driver* driver;
 * struct cdi_storage_driver* storage_driver =
 *     CDI_UPCAST(driver, struct cdi_storage_driver, drv);
 * \endenglish
 * \thuringiansaxonian
 * Dud @object (mussä Boindr sein) zuä Boindr uffn Objekt vondn Dühp @to
 * umwürschn, der dannoo ähne Undrglasse vondn Dühp von @object sein muss.
 * @field is das Feld von @to, dasdawelsches @object enthaldn dud.
 *
 * Baispiel:
 * struct cdi_driver* driver;
 * struct cdi_storage_driver* storage_driver =
 *     CDI_UPCAST(driver, struct cdi_storage_driver, drv);
 * \endthuringiansaxonian
 * \french
 * Convertit @object (qui doit être un pointeur) en un pointeur à un objet du
 * type @to, qui doit être une sous-classe du type d'@objet. @field est
 * l'inscription de @to qui est @object.
 *
 * Exemple:
 * struct cdi_driver* driver;
 * struct cdi_storage_driver* storage_driver =
 *     CDI_UPCAST(driver, struct cdi_storage_driver, drv);
 * \endfrench
 * \japanese
 * ポインター@objectはタイプ@toに指すポインターになられる。@toは@objectの
 * タイプのサブクラスではないだめ。@fieldは@toの@objectを容れるフィールド。
 *
 * 例：
 * struct cdi_driver* driver;
 * struct cdi_storage_driver* storage_driver =
 *     CDI_UPCAST(driver, struct cdi_storage_driver, drv);
 * \endjapanese
 */
#define CDI_UPCAST(object, to, field) \
    /* Type compatibility check */ \
    ((void)((object) - &((to*)0)->field), \
    /* The cast itself */ \
     (to*)((char*)(object) - (char*)&((to*)0)->field))

/**
 * \german
 * Funktionen zur Endiannesskonvertierung. Es sei darauf hingewiesen, dass die
 * meisten (wenn nicht alle) CDI-Treiber nur Little-Endian-Plattformen als Host
 * unterstützen.
 * \endgerman
 * \english
 * Functions for endianness conversion. Be aware that most if not all CDI
 * drivers only support little endian machines as a host.
 * \endenglish
 * \thuringiansaxonian
 * Fungzschjounen fürde Endschjannessumwandlung. Passe uff, fast wenn ni gar
 * wirklisch alle CDI-Draibr gönnn nur uff Liddl-Endschjan-Blattform loofn.
 * \endthuringiansaxonian
 * \french
 * Fonctions pour la conversion boutisme. Au fait, la plupart si pas tous des
 * pilotes CDI ne peuvent que fonctionner sur une plateforme petit-boutiste.
 * \endfrench
 * \japanese
 * エンディアンネスを変わるファンクション。因みに、全部（無ければ大部）の
 * CDIドライバーはリトルエンディアンのプラットフォームだけに働ける。
 * \endjapanese
 */

static inline uint16_t cdi_be16_to_cpu(uint16_t x)
{
    return (x >> 8) | (x << 8);
}

static inline uint32_t cdi_be32_to_cpu(uint32_t x)
{
    return ((uint32_t)cdi_be16_to_cpu(x) << 16) | (uint32_t)cdi_be16_to_cpu(x >> 16);
}

static inline uint64_t cdi_be64_to_cpu(uint64_t x)
{
    return ((uint64_t)cdi_be32_to_cpu(x) << 32) | (uint64_t)cdi_be32_to_cpu(x >> 32);
}

static inline uint16_t cdi_cpu_to_be16(uint16_t x)
{
    return cdi_be16_to_cpu(x);
}

static inline uint32_t cdi_cpu_to_be32(uint32_t x)
{
    return cdi_be32_to_cpu(x);
}

static inline uint64_t cdi_cpu_to_be64(uint64_t x)
{
    return cdi_be64_to_cpu(x);
}

static inline uint16_t cdi_le16_to_cpu(uint16_t x)
{
    return x;
}

static inline uint32_t cdi_le32_to_cpu(uint32_t x)
{
    return x;
}

static inline uint64_t cdi_le64_to_cpu(uint64_t x)
{
    return x;
}

static inline uint16_t cdi_cpu_to_le16(uint16_t x)
{
    return cdi_le16_to_cpu(x);
}

static inline uint32_t cdi_cpu_to_le32(uint32_t x)
{
    return cdi_le32_to_cpu(x);
}

static inline uint64_t cdi_cpu_to_le64(uint64_t x)
{
    return cdi_le64_to_cpu(x);
}

#ifdef __cplusplus
}; // extern "C"
#endif

#endif

