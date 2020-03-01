/*******************************************************************************
 * Copyright (c) 2010-2020 Robert Bosch GmbH
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *    Robert Bosch GmbH - implement Quectel BG96 variant
 *
 ******************************************************************************/

/**
 * @ingroup KISO_CELLULAR_VARIANT_QUECTEL
 * @defgroup KISO_CELLULAR_QUECTEL_ATQUECTEL Quectel AT Commands
 * @{
 *
 * @brief AT commands specific to Quectel modems. Refer to Quectel datasheet for
 * more details.
 *
 * The purpose of this API is to map the Quectel specific AT text-commands
 * into strongly typed C-code. The API tries to closely mimic the AT text
 * interface -- with all its quirks and oddities. Some parts of this API may
 * therefore seem odd to normal C developers. Keep in mind that this API was
 * written for someone who has read the Quectel AT command reference manual,
 * played with some dev-kit, and entered AT commands via serial terminal.
 *
 * @file
 */
#ifndef ATQUECTEL_H_
#define ATQUECTEL_H_

#include "Kiso_Retcode.h"

#include "AtTransceiver.h"

/**
 * @brief Maximum number of characters allowed to be submitted during
 * `AT+QCFG="nwscanseq",<scanseq>`.
 *
 * @warning Unfortunately, Quectel's datasheet does not define a specific limit
 * on the scan sequence length. The number here has been determined through
 * trial and error using a BG96MA-128-SGN. In practice, such a long scan
 * sequence is probably impractical anyway. Still, the limit may depend on the
 * modem's firmware configuration and could change accross revisions. Keep that
 * in mind when planing on defining exceedingly long scan sequences.
 */
#define ATQUECTEL_QCFG_MAXSCANSEQLENGTH (20U)
/**
 * @brief Maximum of characters for an ICCID returned by `AT+QCCID`. Note that
 * some ICCID's may be shorter than that.
 */
#define ATQUECTEL_QCCID_MAXLENGTH (20U)
/**
 * @brief Initializer for the default localhost IPv4 address. Useful in
 * #AtQuectel_SetQIOPEN(), when creating a `TCP LISTENER` or `UDP SERVICE`.
 */
#define ATQUECTEL_LOCALIP ((AtQuectel_Address_T){ \
    .Type = ATQUECTEL_ADDRESSTYPE_IPV4,           \
    .Address.IPv4[3] = 127,                       \
    .Address.IPv4[2] = 0,                         \
    .Address.IPv4[1] = 0,                         \
    .Address.IPv4[0] = 1,                         \
})

/**
 * @brief Enum representation of the various settings configurable via
 * `AT+QCFG=...`. Each value represents the string value set as first parameter
 * during `QCFG`-set commands.
 *
 * Used by #AtQuectel_SetQCFG() for selecting the right union member in @p param
 * and @p resp.
 */
enum AtQuectel_QCFG_Setting_E
{
    ATQUECTEL_QCFG_SETTING_NWSCANMODE,       //!< Specifies the searching sequence of RATs.
                                             //!< See #AtQuectel_QCFG_Set_S::AtQuectel_QCFG_SetParameters_U::NwScanMode.
    ATQUECTEL_QCFG_SETTING_NWSCANSEQ,        //!< Specifies the RAT(s) allowed to be searched.
                                             //!< See #AtQuectel_QCFG_Set_S::AtQuectel_QCFG_SetParameters_U::NwScanSeq.
    ATQUECTEL_QCFG_SETTING_SERVICEDOMAIN,    //!< Specifies the registered service domain.
    ATQUECTEL_QCFG_SETTING_ROAMSERVICE,      //!< Used to enable or disable roam servicing.
    ATQUECTEL_QCFG_SETTING_BAND,             //!< Specifies the bands allowed to be searched by the UE.
    ATQUECTEL_QCFG_SETTING_MSC,              //!< Specifies the UE MSC release version.
    ATQUECTEL_QCFG_SETTING_SGSN,             //!< Specifies the UE SGSN release version.
    ATQUECTEL_QCFG_SETTING_CELEVEL,          //!< Read the LTE Cat. NB1 coverage enhancement level.
    ATQUECTEL_QCFG_SETTING_PDPDUPLICATECHK,  //!< Allows/refuses establishing multiple PDNs with the same APN profile.
    ATQUECTEL_QCFG_SETTING_IOTOPMODE,        //!< Specifies the network category to be searched under LTE RAT.
                                             //!< See #AtQuectel_QCFG_Set_S::AtQuectel_QCFG_SetParameters_U::IoTOpMode.
    ATQUECTEL_QCFG_SETTING_NB1BANDPRIOR,     //!< Specifies the band to be scanned with priority under LTE Cat. NB1 RAT.
    ATQUECTEL_QCFG_SETTING_PSMURC,           //!< Enable or disable the output of URC `+QPSMTIMER`.
    ATQUECTEL_QCFG_SETTING_URCRIRING,        //!< Specifies the @c RI behavior when URC `RING` is presented to indicate an incoming call.
    ATQUECTEL_QCFG_SETTING_URCRISMSINCOMING, //!< Specifies the @c RI behavior when `+CMTI`, `+CMT`, `+CDS` or `+CBM` URCs are presented.
    ATQUECTEL_QCFG_SETTING_URCRIOTHER,       //!< Specifies the @c RI behavior when other URCs are presented.
    ATQUECTEL_QCFG_SETTING_RISIGNALTYPE,     //!< Specifies the @c RI behavior signal output carrier.
    ATQUECTEL_QCFG_SETTING_URCDELAY,         //!< Specifies delay of URC indication until ring indicator pulse ends.
    ATQUECTEL_QCFG_SETTING_IMS,              //!< Used to enable or disable IMS function.
    ATQUECTEL_QCFG_SETTING_LEDMODE,          //!< Used to configure the output mode of @c NETLIGHT pin.
    ATQUECTEL_QCFG_SETTING_CMUXURCPORT,      //!< Select the URC output port for `CMUX`.
    ATQUECTEL_QCFG_SETTING_APREADY,          //!< Used to configure the `AP_READY` pin behavior.
    ATQUECTEL_QCFG_SETTING_PSMENTER,         //!< Used to trigger the module to enter PSM immediately after RRC release.
    ATQUECTEL_QCFG_SETTING_RRCABORT,         //!< Used to abort RCC connection.
    ATQUECTEL_QCFG_SETTING_NCCCONF,          //!< Used to configure features of NB-IoT.

    ATQUECTEL_QCFG_SETTING_MAX,                                 //!< Marks the end of this enum (used for iterating loops). Identical to #ATQUECTEL_QCFG_SETTING_INVALID.
    ATQUECTEL_QCFG_SETTING_INVALID = ATQUECTEL_QCFG_SETTING_MAX //!< Marks and invalid `QCFG`-setting. Identical to #ATQUECTEL_QCFG_SETTING_MAX.
};
typedef enum AtQuectel_QCFG_Setting_E AtQuectel_QCFG_Setting_T;

/**
 * @brief Enum representation of desired network scan modes, configured via
 * `AT+QCFG="nwscanmode",<scanmode>`.
 */
enum AtQuectel_QCFG_NwScanMode_ScanMode_E
{
    ATQUECTEL_QCFG_SCANMODE_AUTOMATIC = 0, //!< Automatic RAT selection.
    ATQUECTEL_QCFG_SCANMODE_GSMONLY = 1,   //!< GSM only.
    /* gap */
    ATQUECTEL_QCFG_SCANMODE_LTEONLY = 3, //!< LTE only.

    ATQUECTEL_QCFG_SCANMODE_INVALID //!< Represents an invalid `<scanmode>`.
};
typedef enum AtQuectel_QCFG_NwScanMode_ScanMode_E AtQuectel_QCFG_NwScanMode_ScanMode_T;

/**
 * @brief Configuration parameters for `AT+QCFG="nwscanmode",...`.
 */
struct AtQuectel_QCFG_NwScanMode_S
{
    AtQuectel_QCFG_NwScanMode_ScanMode_T ScanMode; //!< Scan mode parameter. Set this to #ATQUECTEL_QCFG_SCANMODE_INVALID to read the currently configured value.
    bool TakeEffectImmediately;                    //!< @brief When set to `false` config will take effect only after UE reboots.
                                                   //!< Attribute ignored if setting parameter is being read.
};
typedef struct AtQuectel_QCFG_NwScanMode_S AtQuectel_QCFG_NwScanMode_T;

/**
 * @brief Configuration parameters for `AT+QCFG="nwscanseq",...`.
 */
struct AtQuectel_QCFG_NwScanSeq_S
{
    char ScanSeq[ATQUECTEL_QCFG_MAXSCANSEQLENGTH + 1]; //!< @brief RAT search sequence (e.g. `"020301"` stands for LTE Cat. M1 -> LTE Cat. NB1 -> GSM).
                                                       //!<
                                                       //!< | Token | Meaning      |
                                                       //!< | :---- | :----------- |
                                                       //!< | 00    | Automatic    |
                                                       //!< | 01    | GSM          |
                                                       //!< | 02    | LTE Cat. M1  |
                                                       //!< | 03    | LTE Cat. NB1 |
                                                       //!<
                                                       //!< Must be zero-terminated.
    bool TakeEffectImmediately;                        //!< @brief When set to `false` config will take effect only after UE reboots.
                                                       //!< Attribute ignored if setting parameter is being read.
};
typedef struct AtQuectel_QCFG_NwScanSeq_S AtQuectel_QCFG_NwScanSeq_T;

/**
 * @brief Enum representation of desired network category to be searched
 * under LTE RAT, configured via `AT+QCFG="iotopmode",<mode>`.
 */
enum AtQuectel_QCFG_IoTOpMode_Mode_E
{
    ATQUECTEL_QCFG_IOTOPMODE_LTECATM1 = 0,             //!< LTE Cat. M1
    ATQUECTEL_QCFG_IOTOPMODE_LTECATNB1 = 1,            //!< LTE Cat. NB1
    ATQUECTEL_QCFG_IOTOPMODE_LTECATM1ANDLTECATNB1 = 2, //!< LTE Cat. M1 and LTE Cat. NB1

    ATQUECTEL_QCFG_IOTOPMODE_INVALID //!< Represents an invalid `<mode>`.
};
typedef enum AtQuectel_QCFG_IoTOpMode_Mode_E AtQuectel_QCFG_IoTOpMode_Mode_T;

/**
 * @brief Configuration parameters for `AT+QCFG="iotopmode",...`.
 */
struct AtQuectel_QCFG_IoTOpMode_S
{
    AtQuectel_QCFG_IoTOpMode_Mode_T Mode; //!< Mode parameter. Set this to #ATQUECTEL_QCFG_IOTOPMODE_INVALID to read the currently configured value.
    bool TakeEffectImmediately;           //!< @brief When set to `false` config will take effect only after UE reboots.
                                          //!< Attribute ignored if setting parameter is being read.
};
typedef struct AtQuectel_QCFG_IoTOpMode_S AtQuectel_QCFG_IoTOpMode_T;

/**
 * @brief Query parameters for `AT+QCFG=<setting>`.
 */
struct AtQuectel_QCFG_Query_S
{
    AtQuectel_QCFG_Setting_T Setting; //!< The `<setting>` parameter used in the query.
};
typedef struct AtQuectel_QCFG_Query_S AtQuectel_QCFG_Query_T;

/**
 * @brief Arguments returned by a `AT+QCFG=<setting>,...` query.
 */
struct AtQuectel_QCFG_QueryResponse_S
{
    AtQuectel_QCFG_Setting_T Setting; //!< Selects which `<setting>` should be configured.
    union {
        AtQuectel_QCFG_NwScanMode_T NwScanMode; //!< Arguments configured in `"nwscanmode"` setting, valid only if #Setting set to #ATQUECTEL_QCFG_SETTING_NWSCANMODE.
        AtQuectel_QCFG_NwScanSeq_T NwScanSeq;   //!< Arguments configured in `"nwscanseq"` setting, valid only if #Setting set to #ATQUECTEL_QCFG_SETTING_NWSCANSEQ.
        AtQuectel_QCFG_IoTOpMode_T IoTOpMode;   //!< Arguments configured in `"iotopmode"` setting, valid only if #Setting set to #ATQUECTEL_QCFG_SETTING_IOTOPMODE.
    } Value;                                    //!< Value depending on #Setting.
};
typedef struct AtQuectel_QCFG_QueryResponse_S AtQuectel_QCFG_QueryResponse_T;

/**
 * @brief Represents the parameters written during
 * `AT+QCFG=<setting>,<parameters...>`.
 */
struct AtQuectel_QCFG_Set_S
{
    AtQuectel_QCFG_Setting_T Setting; //!< Selects which `<setting>` should be configured.
    union AtQuectel_QCFG_SetParameters_U {
        AtQuectel_QCFG_NwScanMode_T NwScanMode; //!< Parameters for `"nwscanmode"` setting, valid only if #Setting set to #ATQUECTEL_QCFG_SETTING_NWSCANMODE.
        AtQuectel_QCFG_NwScanSeq_T NwScanSeq;   //!< Parameters for `"nwscanseq"` setting, valid only if #Setting set to #ATQUECTEL_QCFG_SETTING_NWSCANSEQ.
        AtQuectel_QCFG_IoTOpMode_T IoTOpMode;   //!< Parameters for `"iotopmode"` setting, valid only if #Setting set to #ATQUECTEL_QCFG_SETTING_IOTOPMODE.
    } Value;                                    //!< Value depending on #Setting.
};
typedef struct AtQuectel_QCFG_Set_S AtQuectel_QCFG_Set_T;

/**
 * @brief Represents possible values for `<urcportvalue>` in
 * `AT+QURCCFG="urcport",<urcportvalue>`.
 */
enum AtQuectel_QURCCFG_UrcPortValue_E
{
    ATQUECTEL_QURCCFG_URCPORTVALUE_USBAT,    //!< Use USB AT port for URC output.
    ATQUECTEL_QURCCFG_URCPORTVALUE_USBMODEM, //!< Use USB modem port for URC output.
    ATQUECTEL_QURCCFG_URCPORTVALUE_UART1,    //!< Use main UART for URC output.

    ATQUECTEL_QURCCFG_URCPORTVALUE_INVALID //!< Represents an invalid `<urcportvalue>` value.
};
typedef enum AtQuectel_QURCCFG_UrcPortValue_E AtQuectel_QURCCFG_UrcPortValue_T;

/**
 * @brief Parameters used during `AT+QURCCFG="urcport",<urcportvalue>`.
 */
struct AtQuectel_QURCCFG_UrcPort_S
{
    AtQuectel_QURCCFG_UrcPortValue_T UrcPortValue; //!< Represents the value of `<urcportvalue>`.
};
typedef struct AtQuectel_QURCCFG_UrcPort_S AtQuectel_QURCCFG_UrcPort_T;

/**
 * @brief Represents the `<setting>` value in `AT+QURCCFG=<setting>,...`.
 */
enum AtQuectel_QURCCFG_Setting_E
{
    ATQUECTEL_QURCCFG_SETTING_URCPORT, //!< Represents the `"urcport"` setting.

    ATQUECTEL_QURCCFG_SETTING_INVALID,                                //!< Represents an invalid `<setting>`.
    ATQUECTEL_QURCCFG_SETTING_MAX = ATQUECTEL_QURCCFG_SETTING_INVALID //!< Equivalent to #ATQUECTEL_QURCCFG_SETTING_INVALID. Used for iterating through all available settings.
};
typedef enum AtQuectel_QURCCFG_Setting_E AtQuectel_QURCCFG_Setting_T;

/**
 * @brief Arguments for a `AT+QURCCFG=<setting>` query.
 */
struct AtQuectel_QURCCFG_Query_S
{
    AtQuectel_QURCCFG_Setting_T Setting; //!< Identifies which `<setting>` should be queried.
};
typedef struct AtQuectel_QURCCFG_Query_S AtQuectel_QURCCFG_Query_T;

/**
 * @brief Holds response parameters of `+QURCCFG: <setting>,<parameters...>`
 * query response.
 */
struct AtQuectel_QURCCFG_QueryResponse_S
{
    AtQuectel_QURCCFG_Setting_T Setting; //!< Identifies which setting was returned in the query response.
    union {
        AtQuectel_QURCCFG_UrcPort_T UrcPort; //!< Value for `"urcport"` setting, only valid if #Setting set to #ATQUECTEL_QURCCFG_SETTING_URCPORT.
    } Value;                                 //!< Value depending on #Setting.
};
typedef struct AtQuectel_QURCCFG_QueryResponse_S AtQuectel_QURCCFG_QueryResponse_T;

/**
 * @brief Parameters written during `AT+QURCCFG="urcport"[,urcportvalue>]`.
 */
struct AtQuectel_QURCCFG_Set_S
{
    AtQuectel_QURCCFG_Setting_T Setting; //!< Identifies which setting to write the parameters to.
    union {
        AtQuectel_QURCCFG_UrcPort_T UrcPort; //!< Value for `"urcport"` setting, only valid if #Setting set to #ATQUECTEL_QURCCFG_SETTING_URCPORT.
    } Value;                                 //!< Value depending on #Setting.
};
typedef struct AtQuectel_QURCCFG_Set_S AtQuectel_QURCCFG_Set_T;

/**
 * @brief Holds the response of a `AT+QCCID` action response.
 */
struct AtQuectel_QCCID_ExecuteResponse_S
{
    char Iccid[ATQUECTEL_QCCID_MAXLENGTH + 1]; //!< String containing the ICCID of the (U)SIM card.
                                               //!< @details The string is guaranteed to be zero-terminated if obtained through #AtQuectel_ExecuteQCCID().
                                               //!< @note Depending on the SIM card, the ICCID may be 19 or 20 characters long.
};
typedef struct AtQuectel_QCCID_ExecuteResponse_S AtQuectel_QCCID_ExecuteResponse_T;

/**
 * @brief Represents possible values for the `<urctype>` parameter in
 * `AT+QINDCFG=<urctype>[,<enable>[,<savetonvram>]]`
 */
enum AtQuectel_QINDCFG_UrcType_E
{
    ATQUECTEL_QINDCFG_URCTYPE_ALL,         //!< Main switch for all URCs.
    ATQUECTEL_QINDCFG_URCTYPE_CSQ,         //!< Indication of signal strength and channel bit error rate change.
    ATQUECTEL_QINDCFG_URCTYPE_SMSFULL,     //!< SMS storage full indication.
    ATQUECTEL_QINDCFG_URCTYPE_RING,        //!< `RING` indication.
    ATQUECTEL_QINDCFG_URCTYPE_SMSINCOMING, //!< Incoming SMS indication.

    ATQUECTEL_QINDCFG_URCTYPE_INVALID,                                //!< Represents an invalid `<urctype>`.
    ATQUECTEL_QINDCFG_URCTYPE_MAX = ATQUECTEL_QINDCFG_URCTYPE_INVALID //!< Equivalent to #ATQUECTEL_QINDCFG_URCTYPE_INVALID. Used for iterating through all available enum values.
};
typedef enum AtQuectel_QINDCFG_UrcType_E AtQuectel_QINDCFG_UrcType_T;

/**
 * @brief Represents the query arguments used during
 * `AT+QINDCFG=<urctype>`.
 */
struct AtQuectel_QINDCFG_Query_S
{
    AtQuectel_QINDCFG_UrcType_T UrcType; //!< URC type to query used as `<urctype>`.
};
typedef struct AtQuectel_QINDCFG_Query_S AtQuectel_QINDCFG_Query_T;

/**
 * @brief Represents the parsed response arguments of
 * `+QINDCFG: <urctype>,<enable>`.
 */
struct AtQuectel_QINDCFG_QueryResponse_S
{
    AtQuectel_QINDCFG_UrcType_T UrcType; //!< Represents the `<urctype>` argument.
    bool Enable;                         //!< Represents the `<enable>` argument.
};
typedef struct AtQuectel_QINDCFG_QueryResponse_S AtQuectel_QINDCFG_QueryResponse_T;

/**
 * @brief Represents the arguments used during config write of
 * `AT+QINDCFG=<urctype>[,<enable>[,<savetonvram>]]`.
 */
struct AtQuectel_QINDCFG_Set_S
{
    AtQuectel_QINDCFG_UrcType_T UrcType; //!< Represents teh `<urctype>` argument.
    bool Enable;                         //!< Represents the `<enable>` argument.
    bool SaveToNonVolatileRam;           //!< Represents the `<savetonvram>` argument.
};
typedef struct AtQuectel_QINDCFG_Set_S AtQuectel_QINDCFG_Set_T;

/**
 * @brief Enum representation of the different (U)SIM initialization states.
 *
 * States may be bit-OR'ed together in the modem response.
 */
enum AtQuectel_QINISTAT_Status_E
{
    ATQUECTEL_QINISTAT_STATUS_INITIALSTATE = 0,         //!< Initial state, i.e. SIM is not initialized.
    ATQUECTEL_QINISTAT_STATUS_CPINREADY = 1 << 0,       //!< Locking/unlocking SIM via PIN is now allowed.
    ATQUECTEL_QINISTAT_STATUS_SMSINITCOMPLETE = 1 << 1, //!< SMS initialization complete.
};
typedef enum AtQuectel_QINISTAT_Status_E AtQuectel_QINISTAT_Status_T;

/**
 * @brief Holds response returned from `AT+QINISTAT`.
 */
struct AtQuectel_QINISTAT_ExecuteResponse_S
{
    AtQuectel_QINISTAT_Status_T Status; //!< Bitfield of states.
};
typedef struct AtQuectel_QINISTAT_ExecuteResponse_S AtQuectel_QINISTAT_ExecuteResponse_T;

typedef uint32_t AtQuectel_ContextId_T;
typedef uint32_t AtQuectel_ConnectId_T;

/**
 * @brief Enum representation of data-context types used by Quectel's internal
 * TCP/IP stack.
 */
enum AtQUectel_QICSGP_ContextType_E
{
    ATQUECTEL_QICSGP_CONTEXTTYPE_NOTCONFIGURED = 0, //!< Data-Context is not configured.
    ATQUECTEL_QICSGP_CONTEXTTYPE_IPV4 = 1,          //!< IPv4
    ATQUECTEL_QICSGP_CONTEXTTYPE_IPV6 = 2,          //!< IPv6
    ATQUECTEL_QICSGP_CONTEXTTYPE_IPV4V6 = 3,        //!< IPv4v6
};
typedef enum AtQUectel_QICSGP_ContextType_E AtQUectel_QICSGP_ContextType_T;

/**
 * @brief Represents supported authentication methods by the `+QICSGP` command.
 */
enum AtQuectel_QICSGP_Authentication_E
{
    ATQUECTEL_QICSGP_AUTHENTICATION_NONE = 0,      //!< No authentication.
    ATQUECTEL_QICSGP_AUTHENTICATION_PAP = 1,       //!< Personal Authentication Protocol (PAP).
    ATQUECTEL_QICSGP_AUTHENTICATION_CHAP = 2,      //!< Challenge Handshake Authentication Protocol (CHAP).
    ATQUECTEL_QICSGP_AUTHENTICATION_PAPORCHAP = 3, //!< PAP or CHAP.
};
typedef enum AtQuectel_QICSGP_Authentication_E AtQuectel_QICSGP_Authentication_T;

/**
 * @brief Query parameters for reading the current TCP/IP context parameters of
 * a particular data-context.
 */
struct AtQuectel_QICSGP_Query_S
{
    AtQuectel_ContextId_T ContextId; //!< The ID of the context to query (range 1-16).
};
typedef struct AtQuectel_QICSGP_Query_S AtQuectel_QICSGP_Query_T;

struct AtQuectel_QICSGP_QueryResponse_S
{
    AtQUectel_QICSGP_ContextType_T ContextType;       //!< Protocol type obtained from the response.
    char *Apn;                                        //!< @brief Buffer to store the Access Point Name (APN) into.
                                                      //!< @details Must be allocated by the user @b before initiating
                                                      //!< the query. If set to `NULL`, APN will be dropped from the
                                                      //!< response. The buffer is guaranteed to be zero-terminated
                                                      //!< after successful return from #AtQuectel_QueryQICSGP().
    size_t ApnSize;                                   //!< @brief Allocated size of #Apn.
                                                      //!< @details Must account for zero-termination. Ignored if #Apn
                                                      //!< set to `NULL`. If the buffer is too small to hold the full
                                                      //!< string an error is returned.
    char *Username;                                   //!< @brief Buffer to store the APN username obtained from the
                                                      //!< response.
                                                      //!< @details Must be allocated by the user @b before initiating
                                                      //!< the query. If set to `NULL`, username will be dropped from
                                                      //!< the response. The buffer is guaranteed to be zero-terminated
                                                      //!< after successful return from #AtQuectel_QueryQICSGP().
    size_t UsernameSize;                              //!< @brief Allocated size of #Username.
                                                      //!< @details Must account for zero-termination. Ignored if
                                                      //!< #Username set to `NULL`. If the buffer is too small to hold
                                                      //!< the full string an error is returned.
    char *Password;                                   //!< @brief APN password obtained from the response.
                                                      //!< @details Must be allocated by the user @b before initiating
                                                      //!< the query. If set to `NULL`, password will be dropped from
                                                      //!< the response. The buffer is guaranteed to be zero-terminated
                                                      //!< after successful return from #AtQuectel_QueryQICSGP().
    size_t PasswordSize;                              //!< @brief Allocated size of #Password.
                                                      //!< @details Must account for zero-termination. Ignored if
                                                      //!< #Password set to `NULL`. If the buffer is too small to hold
                                                      //!< the full string an error is returned.
    AtQuectel_QICSGP_Authentication_T Authentication; //!< Authentication method obtained from the response.
};
typedef struct AtQuectel_QICSGP_QueryResponse_S AtQuectel_QICSGP_QueryResponse_T;

/**
 * @brief Configuration parameters for Quectel's internal TCP/IP stack, set via
 * `AT+QICSGP=<contextID>,<context_type>,<APN>[,<username>,<password>][,<authentication>]]`.
 */
struct AtQuectel_QICSGP_Set_S
{
    AtQuectel_ContextId_T ContextId;                  //!< ID of the context to configure (range 1-16).
    AtQUectel_QICSGP_ContextType_T ContextType;       //!< Protocol type to be applied.
    const char *Apn;                                  //!< Access Point Name (APN) to be applied.
    const char *Username;                             //!< @brief Username used for authenticating against the APN.
                                                      //!< @details Ignored if #Authentication is set to
                                                      //!< #ATQUECTEL_QICSGP_AUTHENTICATION_NONE (and may hence be set
                                                      //!< to `NULL`).
    const char *Password;                             //!< @brief Password used for authenticating against the APN.
                                                      //!< @details Ignored if #Authentication is set to
                                                      //!< #ATQUECTEL_QICSGP_AUTHENTICATION_NONE (and may hence be set
                                                      //!< to `NULL`).
    AtQuectel_QICSGP_Authentication_T Authentication; //!< Authentication method used to authenticate against the APN.
};
typedef struct AtQuectel_QICSGP_Set_S AtQuectel_QICSGP_Set_T;

/**
 * @brief Parameters for setting `AT+QACT=<contextID>`.
 */
struct AtQuectel_QIACT_Set_S
{
    AtQuectel_ContextId_T ContextId; //!< ID of the context to activate (range 1-16).
};
typedef struct AtQuectel_QIACT_Set_S AtQuectel_QIACT_Set_T;

/**
 * @brief Enum representation of different Quectel TCP/IP data-context types.
 */
enum AtQuectel_QIACT_ContextType_E
{
    ATQUECTEL_QIACT_CONTEXTTYPE_NONE = 0, //!< No protocol active.
    ATQUECTEL_QIACT_CONTEXTTYPE_IPV4 = 1, //!< IPv4.
    ATQUECTEL_QIACT_CONTEXTTYPE_IPV6 = 2  //!< IPv6.
};
typedef enum AtQuectel_QIACT_ContextType_E AtQuectel_QIACT_ContextType_T;

/**
 * @brief Quectel supported types of IP addresses.
 */
enum AtQuectel_AddressType_E
{
    ATQUECTEL_ADDRESSTYPE_IPV4, //!< IPv4.
    ATQUECTEL_ADDRESSTYPE_IPV6, //!< IPv6.

    ATQUECTEL_ADDRESSTYPE_INVALID //!< Invalid address.
};
typedef enum AtQuectel_AddressType_E AtQuectel_AddressType_T;

/**
 * @brief Quectel specific representation of an IP address.
 */
struct AtQuectel_Address_S
{
    AtQuectel_AddressType_T Type; //< Defines the type of address (protocol) in this structure.
    union {
        uint8_t IPv4[4];  //!< IPv4, only valid if #Type set to #ATQUECTEL_ADDRESSTYPE_IPV4.
        uint16_t IPv6[8]; //!< IPv6, only valid if #Type set to #ATQUECTEL_ADDRESSTYPE_IPV6.
    } Address;            //!< IP address union.
};
typedef struct AtQuectel_Address_S AtQuectel_Address_T;

/**
 * @brief Response arguments obtained from `AT+QIACT?`.
 */
struct AtQuectel_QIACT_GetResponse_S
{
    AtQuectel_ContextId_T ContextId;           //!< @brief ID of the data-context for this response.
                                               //!< @details Set to `0` to indicate empty response.
    bool ContextState;                         //!< State of the data-context (active, inactive).
    AtQuectel_QIACT_ContextType_T ContextType; //!< Protocol type of the data-context.
    AtQuectel_Address_T IpAddress;             //!< Local IP address.
};
typedef struct AtQuectel_QIACT_GetResponse_S AtQuectel_QIACT_GetResponse_T;

/**
 * @brief Set arguments for `AT+QIDEACT=<contextID>`.
 */
struct AtQuectel_QIDEACT_Set_S
{
    AtQuectel_ContextId_T ContextId; //!< Id of the data-context to deactivate.
};
typedef struct AtQuectel_QIDEACT_Set_S AtQuectel_QIDEACT_Set_T;

/**
 * @brief Enum representation of Quectel's supported service types for their
 * internal TCP/IP stack.
 */
enum AtQuectel_QIOPEN_ServiceType_E
{
    ATQUECTEL_QIOPEN_SERVICETYPE_TCP,         //!< TCP client.
    ATQUECTEL_QIOPEN_SERVICETYPE_UDP,         //!< @brief UDP client.
                                              //!< @details Sockets of this type are hard-bound to a single remote IP.
                                              //!< Their local port is chosen randomly by the stack.
    ATQUECTEL_QIOPEN_SERVICETYPE_TCPLISTENER, //!< TCP listener.
    ATQUECTEL_QIOPEN_SERVICETYPE_UDPSERVICE   //!< @brief UDP server/client.
                                              //!< @details Sockets of this type support the classical `sendto()` and
                                              //!< `recvfrom()` pattern. Their local port must be specified by the user.
};
typedef enum AtQuectel_QIOPEN_ServiceType_E AtQuectel_QIOPEN_ServiceType_T;

/**
 * @brief Used to differentiate different endpoint types in
 * #AtQuectel_QIOPEN_Endpoint_S.
 */
enum AtQuectel_QIOPEN_EndpointType_E
{
    ATQUECTEL_QIOPEN_ENDPOINTTYPE_IPADDRESS, //!< AtQuectel_QIOPEN_Endpoint_S::Endpoint is an IP address.
    ATQUECTEL_QIOPEN_ENDPOINTTYPE_DOMAINNAME //!< AtQuectel_QIOPEN_Endpoint_S::Endpoint is a domain name.
};
typedef enum AtQuectel_QIOPEN_EndpointType_E AtQuectel_QIOPEN_EndpointType_T;

/**
 * @brief Represents an IP endpoint that can either be defined through its IP or
 * domain name.
 *
 * Quectel allows for implicit domain-name-lookup in their `+QIOPEN` command.
 * Thus it is possible to connect either by IP or domain name. This structure is
 * used to represent, which of these options should be used, and to not have to
 * rely on plain strings.
 */
struct AtQuectel_QIOPEN_Endpoint_S
{
    AtQuectel_QIOPEN_EndpointType_T Type; //!< Defines which member of #Endpoint is valid.
    union {
        AtQuectel_Address_T IpAddress; //!< IP address representation of the endpoint.
        const char *DomainName;        //!< Domain name representation of endpoint.
    } Endpoint;
};
typedef struct AtQuectel_QIOPEN_Endpoint_S AtQuectel_QIOPEN_Endpoint_T;

/**
 * @brief Quectel's supported data access modes for internal TCP/IP sockets.
 */
enum AtQuectel_DataAccessMode_E
{
    ATQUECTEL_DATAACCESSMODE_BUFFER,     //!< @brief Buffer access mode.
                                         //!< @details Incoming data is buffered by the modem and a URC is triggered.
                                         //!< Data can then afterwards be queried via `AT+QIRD=<connectID>,...`.
    ATQUECTEL_DATAACCESSMODE_DIRECT,     //!< @brief Direct access mode.
                                         //!< @details On incoming data, a URC is triggered, immediately followed by the
                                         //!< payload as raw output. The URC contains information how many bytes after
                                         //!< the URC are to be interpreted as payload.
    ATQUECTEL_DATAACCESSMODE_TRANSPARENT //!< @brief Transparent access mode.
                                         //!< @details As soon as this mode is applied, the AT interface will enter the
                                         //!< `CONNECT` state in which any bytes sent or received over the
                                         //!< communications interface are to be interpreted as payload. During
                                         //!< `CONNECT` state, AT communication is suspended. Only after exiting
                                         //!< transparent mode (via a specific escape sequence), can AT communication
                                         //!< resume.
                                         //!< @note Due to its massive impact on the communications channel, it is
                                         //!< adviced to only use this mode in a multiplexed environment (i.e. `+CMUX`).
};
typedef enum AtQuectel_DataAccessMode_E AtQuectel_DataAccessMode_T;

/**
 * @brief Parameters supplied to
 * `AT+QIOPEN=<contextID>,<connectID>,<service_type>,<IP_address>/<domain_name>,<remote_port>[,<local_port>[,<access_mode>]]`.
 *
 * @note Eventhough the TA defines `<local_port>` and `<access_mode>` optional,
 * due to limitations in the API, they must always be set when calling
 * `AT+QIOPEN=...`.
 */
struct AtQuectel_QIOPEN_Set_S
{
    AtQuectel_ContextId_T ContextId;            //!< @brief ID of the data-context on which this socket should operate
                                                //!< (range: 1-16).
    AtQuectel_ConnectId_T ConnectId;            //!< ID of the socket-slot (range: 0-11).
    AtQuectel_QIOPEN_ServiceType_T ServiceType; //!< Socket service type.
    AtQuectel_QIOPEN_Endpoint_T RemoteEndpoint; //!< @brief Remote IP or domain name.
                                                //!< @details If #ServiceType is set to
                                                //!< #ATQUECTEL_QIOPEN_SERVICETYPE_UDPSERVICE, this must be set to the
                                                //!< localhost IP address (for convenience use #ATQUECTEL_LOCALIP).
    uint16_t RemotePort;                        //!< @brief Remote port.
                                                //!< @details If #ServiceType is set to
                                                //!< #ATQUECTEL_QIOPEN_SERVICETYPE_UDPSERVICE, this argument will have
                                                //!< practically no effect, as the remote port must always be supplied
                                                //!< during `AT+QISEND=...`. Still, it is stored internally, and can
                                                //!< be queried using `AT+QISTATE?`.
    uint16_t LocalPort;                         //!< Local port.
    AtQuectel_DataAccessMode_T AccessMode;      //!< Socket's Data access mode on creation.
};
typedef struct AtQuectel_QIOPEN_Set_S AtQuectel_QIOPEN_Set_T;

/**
 * @brief Parameters supplied to `AT+QICLOSE=<connectID>[,<timeout>]`
 */
struct AtQuectel_QICLOSE_Set_S
{
    AtQuectel_ConnectId_T ConnectId; //!< ID of the socket to close (range: 0-11).
    uint16_t Timeout;                //!< @brief Number of seconds the modem will wait for a `FIN ACK` flag (in case of
                                     //!< TCP), before aborting.
                                     //!< @details The TA prompt is halted while the modem is waiting for `FIN ACK`.
                                     //!< Setting this to a value of `0` effectively disables the timeout and the TA
                                     //!< will respond with a final response code immediately. This setting has no
                                     //!< effect on connectionless services such as UDP.
};
typedef struct AtQuectel_QICLOSE_Set_S AtQuectel_QICLOSE_Set_T;

/**
 * @brief Enum representation of all possible `<service_type>`'s returned by
 * `AT+QISTATE?` and `AT+QISTATE=<query_type>,<contextID/connectID>`.
 *
 * @see #AtQuectel_QISTATE_GetAndQueryResponse_S
 */
enum AtQuectel_QISTATE_ServiceType_E
{
    ATQUECTEL_QISTATE_SERVICETYPE_TCP,         //!< @brief TCP client.
                                               //!< @see #ATQUECTEL_QIOPEN_SERVICETYPE_TCP
    ATQUECTEL_QISTATE_SERVICETYPE_UDP,         //!< @brief UDP client.
                                               //!< @see #ATQUECTEL_QIOPEN_SERVICETYPE_UDP
    ATQUECTEL_QISTATE_SERVICETYPE_TCPLISTENER, //!< @brief TCP listener.
                                               //!< @see #ATQUECTEL_QIOPEN_SERVICETYPE_TCPLISTENER
    ATQUECTEL_QISTATE_SERVICETYPE_TCPINCOMING, //!< @brief TCP incoming connection.
                                               //!< @details The result of an accepted incoming connection from a
                                               //!< #ATQUECTEL_QIOPEN_SERVICETYPE_TCPLISTENER socket. Behaves the same
                                               //!< as #ATQUECTEL_QIOPEN_SERVICETYPE_TCP. This additional type only
                                               //!< serves as information on the origin of a socket.
    ATQUECTEL_QISTATE_SERVICETYPE_UDPSERVICE   //!< @brief UDP server/client.
                                               //!< @see #ATQUECTEL_QIOPEN_SERVICETYPE_UDPSERVICE
};
typedef enum AtQuectel_QISTATE_ServiceType_E AtQuectel_QISTATE_ServiceType_T;

/**
 * @brief Enum representation of Quectel's socket states as returned by the
 * `+QISTATE` set of commands.
 *
 * @see #AtQuectel_QISTATE_GetAndQueryResponse_S
 */
enum AtQuectel_QISTATE_SocketState_E
{
    ATQUECTEL_QISTATE_SOCKETSTATE_INITIAL = 0,   //!< @brief Initial state.
                                                 //!< @details Connection has not been established.
    ATQUECTEL_QISTATE_SOCKETSTATE_OPENING = 1,   //!< Opening state.
                                                 //!< @details Client is connecting or server is trying to listen.
    ATQUECTEL_QISTATE_SOCKETSTATE_CONNECTED = 2, //!< @brief Connected state.
                                                 //!< @details Client/incoming connection has been established.
    ATQUECTEL_QISTATE_SOCKETSTATE_LISTENING = 3, //!< @brief Listening state.
                                                 //!< @details Server is listening.
    ATQUECTEL_QISTATE_SOCKETSTATE_CLOSING = 4,   //!< @brief Closing state.
                                                 //!< @details Connection is closing.

    ATQUECTEL_QISTATE_SOCKETSTATE_INVALID //!< Invalid `<socket_state>` value.
};
typedef enum AtQuectel_QISTATE_SocketState_E AtQuectel_QISTATE_SocketState_T;

/**
 * @brief Represents possible values for `<at_port>` in response of `+QISTATE`
 * set of commands.
 *
 * @see #AtQuectel_QISTATE_GetAndQueryResponse_S
 */
enum AtQuectel_QISTATE_AtPort_E
{
    ATQUECTEL_QISTATE_ATPORT_USBAT,    //!< USB AT port.
    ATQUECTEL_QISTATE_ATPORT_USBMODEM, //!< USB modem port.
    ATQUECTEL_QISTATE_ATPORT_UART1,    //!< Main UART.

    ATQUECTEL_QISTATE_ATPORT_INVALID //!< Represents an invalid `<at_port>` value.
};
typedef enum AtQuectel_QISTATE_AtPort_E AtQuectel_QISTATE_AtPort_T;

/**
 * @brief Response arguments from `AT+QISTATE?` and
 * `AT+QISTATE=<query_type>,<contextID/connectID>`.
 *
 * @see #AtQuectel_QueryQISTATE()
 * @see #AtQuectel_GetQISTATE()
 */
struct AtQuectel_QISTATE_GetAndQueryResponse_S
{
    AtQuectel_ConnectId_T ConnectId;             //!< Socket service index (range: 0-11).
    AtQuectel_QISTATE_ServiceType_T ServiceType; //!< Service of the socket.
    AtQuectel_Address_T IpAddress;               //!< @brief IP address associated with this socket (interpretation
                                                 //!< depends on #ServiceType).
                                                 //!< @details
                                                 //!< | Service Type | Interpretation |
                                                 //!< |:------------ |:-------------- |
                                                 //!< | #ATQUECTEL_QISTATE_SERVICETYPE_TCP and #ATQUECTEL_QISTATE_SERVICETYPE_UDP | IP address of the remote server the socket is connected/fixed to. |
                                                 //!< | #ATQUECTEL_QISTATE_SERVICETYPE_TCPLISTENER and #ATQUECTEL_QISTATE_SERVICETYPE_UDPSERVICE | Local IP address of the socket. |
                                                 //!< | #ATQUECTEL_QISTATE_SERVICETYPE_TCPINCOMING | IP address of the remote client the socket is connected to. |
    uint16_t RemotePort;                         //!< @brief Remote port number (interpretation depends on
                                                 //!< #ServiceType).
                                                 //!< @details
                                                 //!< | Service Type | Interpretation |
                                                 //!< |:------------ |:-------------- |
                                                 //!< | #ATQUECTEL_QISTATE_SERVICETYPE_TCP and #ATQUECTEL_QISTATE_SERVICETYPE_UDP | Port of the remote server. |
                                                 //!< | #ATQUECTEL_QISTATE_SERVICETYPE_TCPLISTENER and #ATQUECTEL_QISTATE_SERVICETYPE_UDPSERVICE | N.A. (should be ignored) |
                                                 //!< | #ATQUECTEL_QISTATE_SERVICETYPE_TCPINCOMING | Port of the remote client. |
    uint16_t LocalPort;                          //!< @brief Local port number.
                                                 //!< @details A value of `0` means that the local port will be assigned
                                                 //!< automatically.
    AtQuectel_QISTATE_SocketState_T SocketState; //!< State of the socket.
    AtQuectel_ContextId_T ContextId;             //!< Data-Context this socket is associated with.
    AtQuectel_ConnectId_T ServerId;              //!< @brief ID of the listener socket that accepted this socket.
                                                 //!< Only applicable if #ServiceType set to
                                                 //!< #ATQUECTEL_QISTATE_SERVICETYPE_TCPINCOMING.
    AtQuectel_DataAccessMode_T AccessMode;       //!< Currently configured data access mode.
    AtQuectel_QISTATE_AtPort_T AtPort;           //!< COM port of socket service.
};
typedef struct AtQuectel_QISTATE_GetAndQueryResponse_S AtQuectel_QISTATE_GetResponse_T, AtQuectel_QISTATE_QueryResponse_T;

/**
 * @brief Enum representation of all possible `<query_type>` values used in
 * `AT+QISTATE=<query_type>,<contextID/connectID>`.
 *
 * @see #AtQuectel_QueryQISTATE()
 */
enum AtQuectel_QISTATE_QueryType_E
{
    ATQUECTEL_QISTATE_QUERYTYPE_CONTEXTID = 0, //!< @brief Query status of all sockets on a particular data-context.
                                               //!< @note Query may return multiple responses (one for each active
                                               //!< socket on the context). Callers should allocate an appropriately
                                               //!< sized response array.
                                               //!< length `1`.
    ATQUECTEL_QISTATE_QUERYTYPE_CONNECTID = 1, //!< @brief Query status of a particular socket.
                                               //!< @note Only a single response (with information on the requested
                                               //!< socket) will be returned. The response array may therefore be of
                                               //!< length `1`.

    ATQUECTEL_QISTATE_QUERYTYPE_INVALID //!< Invalid `<query_type>` value.
};
typedef enum AtQuectel_QISTATE_QueryType_E AtQuectel_QISTATE_QueryType_T;

/**
 * @brief Parameters needed for a
 * `AT+QISTATE=<query_type>,<contextID/connectID>` query.
 *
 * @see #AtQuectel_QueryQISTATE()
 */
struct AtQuectel_QISTATE_Query_S
{
    AtQuectel_QISTATE_QueryType_T QueryType; //!< Type of this query. Selects which member of #Query is applicable.
    union {
        AtQuectel_ContextId_T ContextId; //!< Context ID of which all active sockets should be listed.
        AtQuectel_ConnectId_T ConnectId; //!< ID of the socket to list.
    } Query;                             //!< Query union.
};
typedef struct AtQuectel_QISTATE_Query_S AtQuectel_QISTATE_Query_T;

/**
 * @brief Parameters needed for sending data over a Quectel socket via
 * `AT+QISEND=<connectID>,<send_length>[,<remoteIP>,<remotePort>]`.
 *
 * The @ref KISO_CELLULAR_QUECTEL_ATQUECTEL API is completely stateless, meaning
 * the function doesn't know of what `<service_type>` the given socket is. Thus
 * relying on the caller to provide parameters `<remoteIP>` and `<remotePort>`
 * only if the socket is of `<service_type>` `"UDP_SERVICE"` (represented by
 * #ATQUECTEL_QISTATE_SERVICETYPE_UDPSERVICE and
 * #ATQUECTEL_QIOPEN_SERVICETYPE_UDPSERVICE ). In case of
 * `<service_type>` `"UDP_SERVICE"` #AtQuectel_QISEND_Set_S::RemoteIp and
 * #AtQuectel_QISEND_Set_S::RemotePort must be valid. Conversely, for sockets of
 * `<service_type>` `"TCP"`, `"TCP INCOMING"` or `"UDP"`
 * #AtQuectel_QISEND_Set_S::RemoteIp address-type should be set to
 * #ATQUECTEL_ADDRESSTYPE_INVALID, thus signalling that both, `<remoteIP>` and
 * `<remotePort>`, should be omitted.
 */
struct AtQuectel_QISEND_Set_S
{
    AtQuectel_ConnectId_T ConnectId; //!< ID of the socket to send on.
    const void *Payload;             //!< Pointer to the payload buffer to be transferred.
    size_t Length;                   //!< Size of the #Payload buffer in bytes.
    AtQuectel_Address_T RemoteIp;    //!< @brief Remote IP to send the data to.
                                     //!< @details Only applicable for sockets of `<service_type>` `"UDP_SERVICE"`, in
                                     //!< which case, #AtQuectel_AddressType_T must be either
                                     //!< #ATQUECTEL_ADDRESSTYPE_IPV4 or #ATQUECTEL_ADDRESSTYPE_IPV6 (i.e. not
                                     //!< #ATQUECTEL_ADDRESSTYPE_INVALID).
    uint16_t RemotePort;             //!< @brief Remote port to send the data to.
                                     //!< @details Only applicable if socket is of `<service_type>` `"UDP_SERVICE"`.
                                     //!< Will be ignored if #RemoteIp is of type #ATQUECTEL_ADDRESSTYPE_INVALID.
};
typedef struct AtQuectel_QISEND_Set_S AtQuectel_QISEND_Set_T;

/**
 * @brief Query parameters for `AT+QISEND=<connectID>,0`.
 */
struct AtQuectel_QISEND_Query_S
{
    AtQuectel_ConnectId_T ConnectId; //!< ID of the socket to query.
};
typedef struct AtQuectel_QISEND_Query_S AtQuectel_QISEND_Query_T;

/**
 * @brief Holds response arguments of `AT+QISEND=<connectID>,0` query.
 */
struct AtQuectel_QISEND_QueryResponse_S
{
    size_t TotalSendLength;     //!< Total number of bytes sent.
    size_t AcknowledgedBytes;   //!< Total number of acknowledged bytes (only applicable for TCP types).
    size_t UnacknowledgedBytes; //!< Total number of unacknowledged bytes (only applicable for TCP types).
};
typedef struct AtQuectel_QISEND_QueryResponse_S AtQuectel_QISEND_QueryResponse_T;

/**
 * @brief Parameters for invoking a `AT+QIRD=<connectID>[,<read_length>]` query.
 */
struct AtQuectel_QIRD_Query_S
{
    AtQuectel_ConnectId_T ConnectId; //!< ID of the socket to read from.
    size_t ReadLength;               //!< @brief Number of bytes to read.
                                     //!< @details The value of this attribute controls what type of query is going to
                                     //!< be sent. If #ReadLength is set to `0`, a statistics query will be performed,
                                     //!< i.e. #AtQuectel_QIRD_QueryResponse_U::Statistics will contain information on
                                     //!< the total number of bytes received, etc. Conversely, if #ReadLength is set to
                                     //!< a value `>0`, a payload query will be performed. For this type of query the
                                     //!< user must allocate a receive buffer beforehand, into which the bytes coming
                                     //!< from the modem can be written. For TCP sockets, the user can control the
                                     //!< number of bytes that should be read from the modem, hence also limiting the
                                     //!< number of bytes written into the receive buffer. On UDP sockets, this is not
                                     //!< possible. The modem will always return a full datagram. Thus the user is
                                     //!< adviced to use the statistics query and allocate a buffer of sufficient size.
                                     //!< To avoid overflows #ReadLength will be used to limit the number of bytes
                                     //!< actually copied into the receive buffer. Any surplus bytes that were
                                     //!< inadvertently read from the modem but cannot be stored in the receive buffer
                                     //!< will be dropped and lost, making the whole datagram unusable.
};
typedef struct AtQuectel_QIRD_Query_S AtQuectel_QIRD_Query_T;

/**
 * @brief Holds response argument of `AT+QIRD=<connectID>[,<read_length>]`
 * query.
 */
struct AtQuectel_QIRD_QueryResponse_U
{
    struct AtQuectel_QIRD_PayloadResponse_S
    {
        size_t ReadActualLength;      //!< Actual number of bytes read from the socket and stored in #Data.
        void *Data;                   //!< @brief Buffer to store the incoming bytes in.
                                      //!< @details This buffer must be allocated by the user @b before invoking the
                                      //!< query.
                                      //!<
                                      //!< If the socket is of type `"UDP"` or `"UDP SERVICE"`, the modem will always
                                      //!< respond with the complete datagram, regardless of the limit set by
                                      //!< `<read_length>`. In such a case #AtQuectel_QIRD_Query_S::ReadLength will be
                                      //!< used to limit the number of bytes copied onto #Data. If the buffer is too,
                                      //!< small surplus bytes will be dropped and lost.
                                      //!<
                                      //!< If set to `NULL`, bytes will be read from the modem, but dropped immediately.
                                      //!< This may be used to clear the modem buffer if one is not interested in the
                                      //!< data.
        AtQuectel_Address_T RemoteIp; //!< @brief IP of the remote endpoint the data was sent from.
                                      //!< @details Only valid if socket is of type `"UDP SERVICE"`. Otherwise,
                                      //!< address-type will be marked #ATQUECTEL_ADDRESSTYPE_INVALID.
        uint16_t RemotePort;          //!< @brief Port of the remote endpoint the data was sent from.
                                      //!< @details Only valid if socket is of type `"UDP SERVICE"` and #RemoteIp marked
                                      //!< as ATQUECTEL_ADDRESSTYPE_IPV4 or ATQUECTEL_ADDRESSTYPE_IPV6.
    } Payload;                        //!< Payload from a read query.
    struct AtQuectel_QIRD_StatisticsResponse_S
    {
        size_t TotalReceiveLength; //!< Total number of bytes received on this socket.
        size_t HaveReadLength;     //!< Total number of bytes read from the modem-side buffer.
        size_t UnreadLength;       //!< Number of bytes currently available to read.
    } Statistics;                  //!< Socket receive statistics.
};
typedef struct AtQuectel_QIRD_QueryResponse_U AtQuectel_QIRD_QueryResponse_T;

/**
 * @brief Write or read Quectel specific configuration parameters to/from modem
 * via `AT+QCFG=...`.
 *
 * Writing or reading the configuration depends on the value of @p param. If
 * configuration parameters are provided (i.e. @p param not `NULL`), @p resp
 * will be ignored and the configuration will be written.
 * Conversely, if no configuration parameters are provided (i.e. @p param set to
 * `NULL`), the modem configuration will be read out and stored in @p resp. In
 * this case @p resp must point to a valid #AtQuectel_QCFG_QueryResponse_T to
 * store the values in.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] query
 * Selects which setting should be read. Determines which members of the
 * response structure will be populated.
 *
 * @param[out] resp
 * Response buffer to store the received configuration values in.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_QueryQCFG(struct AtTransceiver_S *t, const AtQuectel_QCFG_Query_T *query, AtQuectel_QCFG_QueryResponse_T *resp);

/**
 * @brief Write Quectel specific configuration parameters onto the modem via
 * `AT+QCFG=<setting>,<parameters...>`.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] set
 * Configuration values to be written onto the modem.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_SetQCFG(struct AtTransceiver_S *t, const AtQuectel_QCFG_Set_T *set);

/**
 * @brief Read Quectel specific URC configuration parameters from modem via
 * `AT+QURCCFG=<setting>`.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] query
 * Selects which setting should be read. Determines which members of the
 * response structure will be populated.
 *
 * @param[out] resp
 * Response buffer to store the received configuration values in.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_QueryQURCCFG(struct AtTransceiver_S *t, const AtQuectel_QURCCFG_Query_T *query, AtQuectel_QURCCFG_QueryResponse_T *resp);

/**
 * @brief Write Quectel specific URC configuration parameters onto modem via
 * `AT+QURCCFG=<setting>,<parameters...>`.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] set
 * Configuration values to be written onto the modem.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_SetQURCCFG(struct AtTransceiver_S *t, const AtQuectel_QURCCFG_Set_T *set);

/**
 * @brief Execute poll of the ICCID (Integrated Circuit Card Identifier) from
 * the (U)SIM card.
 *
 * SIM card must be present and initialized for this command to succeed. An
 * error may indicate a missing, uninitialized or faulty SIM card.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[out] resp
 * Response buffer to store the queried ICCID value in.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_ExecuteQCCID(struct AtTransceiver_S *t, AtQuectel_QCCID_ExecuteResponse_T *resp);

/**
 * @brief Read the current URC indication state for a given URC group.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] query
 * Selects which URC group should be read.
 *
 * @param[out] resp
 * Response buffer to store the received configuration values in.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_QueryQINDCFG(struct AtTransceiver_S *t, const AtQuectel_QINDCFG_Query_T *query, AtQuectel_QINDCFG_QueryResponse_T *resp);

/**
 * @brief Write URC indication configuration.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] set
 * Configuration values to be written onto the modem.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_SetQINDCFG(struct AtTransceiver_S *t, const AtQuectel_QINDCFG_Set_T *set);

/**
 * @brief Query initialization status of the (U)SIM card.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[out] resp
 * Response buffer to store the received SIM states.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_ExecuteQINISTAT(struct AtTransceiver_S *t, AtQuectel_QINISTAT_ExecuteResponse_T *resp);

/**
 * @brief Query TCP/IP context parameters of a particular data-context.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] query
 * Query parameters to use.
 *
 * @param[out] resp
 * Buffer to store the query result in.
 *
 * @note Some attributes of @p resp must be initialized by the user @b before
 * initiating the query.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_QueryQICSGP(struct AtTransceiver_S *t, const AtQuectel_QICSGP_Query_T *query, AtQuectel_QICSGP_QueryResponse_T *resp);

/**
 * @brief Write TCP/IP context paramters for a particular data-context.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] set
 * Structure containing the TCP/IP context parameters to apply.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_SetQICSGP(struct AtTransceiver_S *t, const AtQuectel_QICSGP_Set_T *set);

/**
 * @brief Activate specific Quectel TCP/IP data-context.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] set
 * Specify which Quectel TCP/IP data-context to activate.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_SetQIACT(struct AtTransceiver_S *t, const AtQuectel_QIACT_Set_T *set);

/**
 * @brief Get all active Quectel TCP/IP data-contexts.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[out] respArray
 * Array of structures used as buffer to store the incoming responses. The
 * array elements are filled in order of the arriving AT responses. If more
 * responses arrive than can be buffered, the surplus responses will be dropped
 * and a warning is returned.
 *
 * @param[in] respSize
 * Element count of @p respArray.
 *
 * @param[out] respLength
 * On return, will be set to the number of received elements in @p respArray.
 * Argument is ignored if set to `NULL`.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 *
 * @retval #RETCODE_OUT_OF_RESOURCES
 * If combined with #RETCODE_SEVERITY_WARNING, indicates that the provided
 * @p respArray was too small to store all incoming responses.
 */
Retcode_T AtQuectel_GetQIACT(struct AtTransceiver_S *t, AtQuectel_QIACT_GetResponse_T *respArray, size_t respSize, size_t *respLength);

/**
 * @brief Deactivate specific Quectel TCP/IP data-context.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] set
 * Specify which Quectel TCP/IP data-context to deactivate.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_SetQIDEACT(struct AtTransceiver_S *t, const AtQuectel_QIDEACT_Set_T *set);

/**
 * @brief Open a new socket using Quectel's internal TCP/IP stack.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] set
 * Parameters used for socket creation.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_SetQIOPEN(struct AtTransceiver_S *t, const AtQuectel_QIOPEN_Set_T *set);

/**
 * @brief Close a socket.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] set
 * Parameters used to select which socket to close.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_SetQICLOSE(struct AtTransceiver_S *t, const AtQuectel_QICLOSE_Set_T *set);

/**
 * @brief Query state all existing sockets.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[out] respArray
 * Array of structures used as buffer to store the incoming responses. The array
 * elements are filled in order of the arriving AT responses. If more responses
 * arrive than can be buffered, the surplus responses will be dropped and a
 * warning is returned.
 *
 * @param[in] respSize
 * Element count of @p respArray.
 *
 * @param[out] respLength
 * On return, will be set to the number of received elements in @p respArray.
 * Argument is ignored if set to `NULL`.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 *
 * @retval #RETCODE_OUT_OF_RESOURCES
 * If combined with #RETCODE_SEVERITY_WARNING, indicates that the provided
 * @p respArray was too small to store all incoming responses.
 */
Retcode_T AtQuectel_GetQISTATE(struct AtTransceiver_S *t, AtQuectel_QISTATE_GetResponse_T *respArray, size_t respSize, size_t *respLength);

/**
 * @brief Query state of single socket or all sockets on a given data-context.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] query
 * Query parameters to use.
 *
 * @param[out] respArray
 * Array of structures used as buffer to store the incoming responses. Depending
 * on the @p query type, this query may return multiple responses. The array
 * elements are filled in order of the arriving AT responses. If more responses
 * arrive than can be buffered, the surplus responses will be dropped and a
 * warning is returned.
 *
 * @param[in] respSize
 * Element count of @p respArray.
 *
 * @param[out] respLength
 * On return, will be set to the number of received elements in @p respArray.
 * Argument is ignored if set to `NULL`.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 *
 * @retval #RETCODE_OUT_OF_RESOURCES
 * If combined with #RETCODE_SEVERITY_WARNING, indicates that the provided
 * @p respArray was too small to store all incoming responses.
 */
Retcode_T AtQuectel_QueryQISTATE(struct AtTransceiver_S *t, const AtQuectel_QISTATE_Query_T *query, AtQuectel_QISTATE_QueryResponse_T *respArray, size_t respSize, size_t *respLength);

/**
 * @brief Send data over a socket.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] set
 * Parameters containing payload buffer and information remote endpoint (if
 * applicable).
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_SetQISEND(struct AtTransceiver_S *t, const AtQuectel_QISEND_Set_T *set);

/**
 * @brief Query send statistics of given socket.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] query
 * Query parameters to use.
 *
 * @param[out] resp
 * Buffer to store the query result in.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T AtQuectel_QueryQISEND(struct AtTransceiver_S *t, const AtQuectel_QISEND_Query_T *query, AtQuectel_QISEND_QueryResponse_T *resp);

/**
 * @brief Read bytes from socket or query receive statistics.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @param[in] query
 * Query parameters to use.
 *
 * @param[out] resp
 * Buffer to store the query result in.
 *
 * @note Some attributes of @p resp must be initialized by the user @b before
 * initiating the query.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 *
 * @retval #RETCODE_OUT_OF_RESOURCES
 * If combined with #RETCODE_SEVERITY_WARNING, indicates that the provided
 * @p resp receive buffer was too small to store the full payload.
 */
Retcode_T AtQuectel_QueryQIRD(struct AtTransceiver_S *t, const AtQuectel_QIRD_Query_T *query, AtQuectel_QIRD_QueryResponse_T *resp);

#endif /* ATQUECTEL_H_ */

/** @} */
