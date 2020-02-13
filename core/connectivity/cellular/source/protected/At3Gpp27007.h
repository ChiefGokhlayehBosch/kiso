/*******************************************************************************
 * Copyright (c) 2010-2019 Robert Bosch GmbH
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *    Robert Bosch GmbH - transceiver based AT command parsing
 *    Robert Bosch GmbH - initial contribution
 *
 ******************************************************************************/

/**
 * @ingroup KISO_CELLULAR_COMMON
 * @defgroup AT3GPP27007 3GPP TS 27.007 AT Commands
 * @{
 * @brief AT commands as specified by 3GPP TS 27.007 V15.4.0 (2018-12).
 * @file
 */
#ifndef AT3GPP27007_H_
#define AT3GPP27007_H_

#include "AtTransceiver.h"
#include "Kiso_Retcode.h"
#include <limits.h>

#define AT3GPP27007_COPS_OPER_FORMAT_LONG_ALPHANUMERIC_MAX_LEN (UINT32_C(16))
#define AT3GPP27007_COPS_OPER_FORMAT_SHORT_ALPHANUMERIC_MAX_LEN (UINT32_C(8))

#define AT3GPP27007_INVALID_LAC (UINT16_MAX)
#define AT3GPP27007_INVALID_CI (UINT32_MAX)
#define AT3GPP27007_INVALID_RAC (UINT8_MAX)
#define AT3GPP27007_INVALID_TAC (UINT16_MAX)

#define AT3GPP27007_URC_CREG ("CREG")
#define AT3GPP27007_URC_CGREG ("CGREG")
#define AT3GPP27007_URC_CEREG ("CEREG")

/**
 * @brief 3GPP TS 27.007 CxREG `<n>` parameter controlling the degree of detail
 * associated with the CxREG URC.
 *
 * Applicable for CREG, CGREG and CEREG.
 */
enum At3Gpp27007_CXREG_N_E
{
    AT3GPP27007_CXREG_N_DISABLED = 0,          //!< URC disabled
    AT3GPP27007_CXREG_N_URC = 1,               //!< Basic URC containing registration info enabled
    AT3GPP27007_CXREG_N_URC_LOC = 2,           //!< Verbose URC containing registration and location info enabled
    AT3GPP27007_CXREG_N_URC_LOC_CAUSE = 3,     //!< (not supported) Verbose URC containing registration, location and cause info enabled
    AT3GPP27007_CXREG_N_URC_LOC_PSM = 4,       //!< (not supported, CGREG and CEREG only) Verbose URC containing registration, location and PSM info enabled
    AT3GPP27007_CXREG_N_URC_LOC_PSM_CAUSE = 5, //!< (not supported, CGREG and CEREG only) Verbose URC containing registration, location, PSM and cause info enabled
    AT3GPP27007_CXREG_N_INVALID = 255          //!< Invalid `<n>` value, used to signal that parameter not present
};
typedef enum At3Gpp27007_CXREG_N_E At3Gpp27007_CXREG_N_T;

/**
 * @brief 3GPP TS 27.007 `AT+CxREG=<n>` set parameters.
 *
 * Applicable for CREG, CGREG and CEREG.
 */
struct At3Gpp27007_CXREG_Set_S
{
    At3Gpp27007_CXREG_N_T N; //!< `<n>` parameter.
};
typedef struct At3Gpp27007_CXREG_Set_S At3Gpp27007_CXREG_Set_T;

/**
 * @brief 3GPP TS 27.007 CxREG `<stat>` parameter.
 *
 * Applicable for CREG, CGREG and CEREG.
 */
enum At3Gpp27007_CXREG_Stat_E
{
    AT3GPP27007_CXREG_STAT_NOT = 0,             //!< Not connected and also not trying to connect to any network
    AT3GPP27007_CXREG_STAT_HOME = 1,            //!< Registered in home network
    AT3GPP27007_CXREG_STAT_NOT_AND_SEARCH = 2,  //!< Not connected but trying to find a network
    AT3GPP27007_CXREG_STAT_DENIED = 3,          //!< Registration denied
    AT3GPP27007_CXREG_STAT_UNKNOWN = 4,         //!< Unknown (e.g out of GERAN/UTRAN&E-UTRAN coverage)
    AT3GPP27007_CXREG_STAT_ROAMING = 5,         //!< Registered to roaming
    AT3GPP27007_CXREG_STAT_SMSONLY_HOME = 6,    //!< Registered to home network for "SMS only" (E-UTRAN only)
    AT3GPP27007_CXREG_STAT_SMSONLY_ROAMING = 7, //!< Registered to roaming network for "SMS only" (E-UTRAN only)
    /* No nb. 8 (attached for emergency bearer services only), note:
     * 3GPP TS 24.008 [8] and 3GPP TS 24.301 [83] specify the condition when
     * the MS is considered as attached for emergency bearer services. */
    AT3GPP27007_CXREG_STAT_CSFB_NOT_PREF_HOME = 9,     //!< Registered to home network for "CSFB not preferred" (E-UTRAN only)
    AT3GPP27007_CXREG_STAT_CSFB_NOT_PREF_ROAMING = 10, //!< Registered to roaming network for "CSFB not preferred" (E-UTRAN only)
    AT3GPP27007_CXREG_STAT_INVALID = 255               //!< Invalid `<stat>` value, used to signal that parameter not present
};
typedef enum At3Gpp27007_CXREG_Stat_E At3Gpp27007_CXREG_Stat_T;

/**
 * @brief 3GPP TS 27.007 CxREG `<AcT>` (Access Technology) parameter of the
 * CxREG command family
 *
 * Applicable for CREG, CGREG and CEREG.
 */
enum At3Gpp27007_CXREG_AcT_E
{
    AT3GPP27007_CXREG_ACT_GSM = 0,               //!< GSM
    AT3GPP27007_CXREG_ACT_GSM_COMPACT = 1,       //!< GSM Compact
    AT3GPP27007_CXREG_ACT_UTRAN = 2,             //!< UTRAN
    AT3GPP27007_CXREG_ACT_GSM_EGPRS = 3,         //!< GSM + EGPRS
    AT3GPP27007_CXREG_ACT_UTRAN_HSDPA = 4,       //!< UTRAN + HSDPA
    AT3GPP27007_CXREG_ACT_UTRAN_HSUPA = 5,       //!< UTRAN + HSUPA
    AT3GPP27007_CXREG_ACT_UTRAN_HSDPA_HSUPA = 6, //!< UTRAN + HSDPA + HSUPA
    AT3GPP27007_CXREG_ACT_EUTRAN = 7,            //!< E-UTRAN
    AT3GPP27007_CXREG_ACT_ECGSMIOT = 8,          //!< EC-GSM-IoT (A/Gb mode)
    AT3GPP27007_CXREG_ACT_EUTRAN_NBS1 = 9,       //!< E-UTRAN (NB-S1 mode)
    AT3GPP27007_CXREG_ACT_EUTRA_5GCN = 10,       //!< E-UTRA connected to a 5GCN
    AT3GPP27007_CXREG_ACT_NR_5GCN = 11,          //!< NR connected to a 5GCN
    AT3GPP27007_CXREG_ACT_NGRAN = 12,            //!< NG-RAN
    AT3GPP27007_CXREG_ACT_EUTRA_NR = 13,         //!< E-UTRAN-NR dual connectivity
    AT3GPP27007_CXREG_ACT_INVALID = 255          //!< Invalid `<AcT>` value, used to signal that parameter not present
};
typedef enum At3Gpp27007_CXREG_AcT_E At3Gpp27007_CXREG_AcT_T;

/**
 * @brief 3GPP TS 27.007 CREG parameters
 */
struct At3Gpp27007_CREG_GetResponse_S
{
    At3Gpp27007_CXREG_N_T N;       //!< Degree of detail.
    At3Gpp27007_CXREG_Stat_T Stat; //!< Current network status.
    uint16_t Lac;                  //!< Location Area Code, range 0h-FFFFh (2 octets).
    uint32_t Ci;                   //!< Cell Identity, range 0h-FFFFFFFFh (4 octets).
    At3Gpp27007_CXREG_AcT_T AcT;   //!< Current Access Technology.
};
typedef struct At3Gpp27007_CREG_GetResponse_S At3Gpp27007_CREG_GetResponse_T, At3Gpp27007_CREG_UrcResponse_T;

/**
 * @brief 3GPP TS 27.007 CGREG parameters
 */
struct At3Gpp27007_CGREG_GetAndUrcResponse_S
{
    At3Gpp27007_CXREG_N_T N;       //!< Degree of detail.
    At3Gpp27007_CXREG_Stat_T Stat; //!< Current network status.
    uint16_t Lac;                  //!< Location Area Code, range 0h-FFFFh (2 octets).
    uint32_t Ci;                   //!< Cell Identity, range 0h-FFFFFFFFh (4 octets).
    At3Gpp27007_CXREG_AcT_T AcT;   //!< Current Access Technology.
    uint8_t Rac;                   //!< Routing Area Code, range 0h-FFh (1 octet).
};
typedef struct At3Gpp27007_CGREG_GetAndUrcResponse_S At3Gpp27007_CGREG_GetResponse_T, At3Gpp27007_CGREG_UrcResponse_T;

/**
 * @brief 3GPP TS 27.007 CEREG parameters
 */
struct At3Gpp27007_CEREG_GetAndUrcResponse_S
{
    At3Gpp27007_CXREG_N_T N;       //!< Degree of detail.
    At3Gpp27007_CXREG_Stat_T Stat; //!< Current network status.
    uint16_t Tac;                  //!< Tracking Area Code, range 0h-FFFFh (2 octets).
    uint32_t Ci;                   //!< Cell Identity, range 0h-FFFFFFFFh (4 octets).
    At3Gpp27007_CXREG_AcT_T AcT;   //!< Current Access Technology.
};
typedef struct At3Gpp27007_CEREG_GetAndUrcResponse_S At3Gpp27007_CEREG_GetResponse_T, At3Gpp27007_CEREG_UrcResponse_T;

/**
 * @brief 3GPP TS 27.007 COPS `<mode>` used to control PLMN (base-station)
 * selection.
 */
enum At3Gpp27007_COPS_Mode_E
{
    AT3GPP27007_COPS_MODE_AUTOMATIC = 0,             //!< Automatic network selection (take what's there).
    AT3GPP27007_COPS_MODE_MANUAL = 1,                //!< Manual network selection (`<oper>` required, `<AcT>` optional).
    AT3GPP27007_COPS_MODE_DEREGISTER = 2,            //!< Deregister from network.
    AT3GPP27007_COPS_MODE_SET_FORMAT_ONLY = 3,       //!< Set the `<format>` field (format of `<oper>` parameter) only, no registration initiated.
    AT3GPP27007_COPS_MODE_MANUAL_THEN_AUTOMATIC = 4, //!< Try manual network registration (`<oper>` field required), fall back to automatic on failed attempt.
    AT3GPP27007_COPS_MODE_INVALID = 255              //!< Invalid `<mode>` value, used to signal that parameter not present.
};
typedef enum At3Gpp27007_COPS_Mode_E At3Gpp27007_COPS_Mode_T;

/**
 * @brief 3GPP TS 27.007 COPS `<format>` used to control the interpretation of
 * the `<oper>` parameter.
 */
enum At3Gpp27007_COPS_Format_E
{
    AT3GPP27007_COPS_FORMAT_LONG_ALPHANUMERIC = 0,  //!< Long alphanumeric format.
    AT3GPP27007_COPS_FORMAT_SHORT_ALPHANUMERIC = 1, //!< Short alphanumeric format.
    AT3GPP27007_COPS_FORMAT_NUMERIC = 2,            //!< Numeric format.
    AT3GPP27007_COPS_FORMAT_INVALID = 255           //!< Invalid `<format>` value, used to signal that parameter not present.
};
typedef enum At3Gpp27007_COPS_Format_E At3Gpp27007_COPS_Format_T;

/**
 * @brief 3GPP TS 27.007 COPS `<oper>` used to specify a PLMN to connect to.
 *
 * @see #At3Gpp27007_COPS_Format_T
 */
union At3Gpp27007_COPS_Oper_U {
    /**
     * @brief Long alphanumeric operator identification.
     *
     * Must be zero-terminated.
     */
    char LongAlphanumeric[AT3GPP27007_COPS_OPER_FORMAT_LONG_ALPHANUMERIC_MAX_LEN + 1];
    /**
     * @brief Short alphanumeric operator identification.
     *
     * Must be zero-terminated.
     */
    char ShortAlphanumeric[AT3GPP27007_COPS_OPER_FORMAT_SHORT_ALPHANUMERIC_MAX_LEN + 1];
    /**
     * @brief Numeric operator identification.
     */
    uint16_t Numeric;
};
typedef union At3Gpp27007_COPS_Oper_U At3Gpp27007_COPS_Oper_T;

/**
 * @brief 3GPP TS 27.007 COPS `<stat>`, currently unused.
 */
enum At3Gpp27007_COPS_Stat_E
{
    AT3GPP27007_COPS_STAT_UNKNOWN = 0,   //!< Status unknown
    AT3GPP27007_COPS_STAT_AVAILABLE = 1, //!< Available
    AT3GPP27007_COPS_STAT_CURRENT = 2,   //!< Currently connected
    AT3GPP27007_COPS_STAT_FORBIDDEN = 3, //!< Forbidden (registration denied)
};
typedef enum At3Gpp27007_COPS_Stat_E At3Gpp27007_COPS_Stat_T;

/**
 * @brief 3GPP TS 27.007 COPS `<AcT>` (Access Technology) parameter of the COPS
 * command.
 */
enum At3Gpp27007_COPS_AcT_E
{
    AT3GPP27007_COPS_ACT_GSM = 0,               //!< GSM
    AT3GPP27007_COPS_ACT_GSM_COMPACT = 1,       //!< GSM Compact
    AT3GPP27007_COPS_ACT_UTRAN = 2,             //!< UTRAN
    AT3GPP27007_COPS_ACT_GSM_EGPRS = 3,         //!< GSM + EGPRS
    AT3GPP27007_COPS_ACT_UTRAN_HSDPA = 4,       //!< UTRAN + HSDPA
    AT3GPP27007_COPS_ACT_UTRAN_HSUPA = 5,       //!< UTRAN + HSUPA
    AT3GPP27007_COPS_ACT_UTRAN_HSDPA_HSUPA = 6, //!< UTRAN + HSDPA + HSUPA
    AT3GPP27007_COPS_ACT_EUTRAN = 7,            //!< E-UTRAN
    AT3GPP27007_COPS_ACT_ECGSMIOT = 8,          //!< EC-GSM-IoT (A/Gb mode)
    AT3GPP27007_COPS_ACT_EUTRAN_NBS1 = 9,       //!< E-UTRAN (NB-S1 mode)
    AT3GPP27007_COPS_ACT_EUTRA_5GCN = 10,       //!< E-UTRA connected to a 5GCN
    AT3GPP27007_COPS_ACT_NR_5GCN = 11,          //!< NR connected to a 5GCN
    AT3GPP27007_COPS_ACT_NGRAN = 12,            //!< NG-RAN
    AT3GPP27007_COPS_ACT_EUTRA_NR = 13,         //!< E-UTRAN-NR dual connectivity
    AT3GPP27007_COPS_ACT_INVALID = 255          //!< Invalid `<AcT>` value, used to signal that parameter not present.
};
typedef enum At3Gpp27007_COPS_AcT_E At3Gpp27007_COPS_AcT_T;

/**
 * @brief 3GPP TS 27.007 `AT+COPS=<mode>[,<format>[,<oper>[,<AcT]]]` set
 * parameters.
 */
struct At3Gpp27007_COPS_Set_S
{
    At3Gpp27007_COPS_Mode_T Mode;     //!< PLMN selection mode.
    At3Gpp27007_COPS_Format_T Format; //!< @brief Format of #Oper.
                                      //!< @details Only applicable if #Mode set to #AT3GPP27007_COPS_MODE_MANUAL,
                                      //!< #AT3GPP27007_COPS_MODE_MANUAL_THEN_AUTOMATIC or
                                      //!< #AT3GPP27007_COPS_MODE_SET_FORMAT_ONLY, otherwise ignored.
    At3Gpp27007_COPS_Oper_T Oper;     //!< Operator filter.
                                      //!< @details Only applicable if #Mode set to #AT3GPP27007_COPS_MODE_MANUAL or
                                      //!< #AT3GPP27007_COPS_MODE_MANUAL_THEN_AUTOMATIC, otherwise ignored.
    At3Gpp27007_COPS_AcT_T AcT;       //!< Access Technology to use.
                                      //!< @details Only applicable if #Mode set to #AT3GPP27007_COPS_MODE_MANUAL or
                                      //!< #AT3GPP27007_COPS_MODE_MANUAL_THEN_AUTOMATIC, otherwise ignored.
};
typedef struct At3Gpp27007_COPS_Set_S At3Gpp27007_COPS_Set_T;

/**
 * @brief 3GPP TS 27.007 CGDCONT `<PDP_Type>` (Packet Data Protocol) parameter
 * used to specify the type of data protocol.
 */
enum At3Gpp27007_CGDCONT_PdpType_E
{
    AT3GPP27007_CGDCONT_PDPTYPE_X25,          //!< ITU-T/CCITT X.25 layer 3 (obsolete, not supported)
    AT3GPP27007_CGDCONT_PDPTYPE_IP,           //!< Internet protocol
    AT3GPP27007_CGDCONT_PDPTYPE_IPV6,         //!< Internet protocol version 6
    AT3GPP27007_CGDCONT_PDPTYPE_IPV4V6,       //!< Virtual type used for dual stack IP
    AT3GPP27007_CGDCONT_PDPTYPE_OSPIH,        //!< Internet Hosted Octect Stream Protocol (obsolete, not supported)
    AT3GPP27007_CGDCONT_PDPTYPE_PPP,          //!< Point to Point Protocol (not supported)
    AT3GPP27007_CGDCONT_PDPTYPE_NONIP,        //!< Transfer of Non-IP data to external packet data network (not supported)
    AT3GPP27007_CGDCONT_PDPTYPE_ETHERNET,     //!< Ethernet protocol (not supported)
    AT3GPP27007_CGDCONT_PDPTYPE_UNSTRUCTURED, //!< Transfer of Unstructured data to the Data Network via N6 (not supported)

    AT3GPP27007_CGDCONT_PDPTYPE_RESET,                                      //!< Used to reset all context settings.
    AT3GPP27007_CGDCONT_PDPTYPE_INVALID = AT3GPP27007_CGDCONT_PDPTYPE_RESET //!< Invalid `<PDP_Type>` value, used to signal that parameter not present.
};
typedef enum At3Gpp27007_CGDCONT_PdpType_E At3Gpp27007_CGDCONT_PdpType_T;

/**
 * @brief 3GPP TS 27.007 `AT+CGDCONT=<cid>[,<PDP_type>[,<APN>]]`
 * set parameters.
 *
 * @note This is only a small subset of the available parameters. Implement more
 * as needed.
 */
struct At3Gpp27007_CGDCONT_Set_S
{
    /**
     * @brief Context-Id to identify the context to apply the settings to.
     * Must be valid Context-Id.
     */
    uint8_t Cid;
    /**
     * @brief Type of PDP context.
     *
     * If set to #AT3GPP27007_CGDCONT_PDPTYPE_RESET all context settings will be
     * reset.
     */
    At3Gpp27007_CGDCONT_PdpType_T PdpType;
    /**
     * @brief APN of this context.
     *
     * Can be set to `NULL` to skip this parameter. Can be set to an empty
     * string to request the APN during network registration.
     */
    const char *Apn;
};
typedef struct At3Gpp27007_CGDCONT_Set_S At3Gpp27007_CGDCONT_Set_T;

/**
 * @brief 3GPP TS 27.007 CGACT `<state>` parameter used to control the state of
 * a data-context.
 */
enum At3Gpp27007_CGACT_State_E
{
    AT3GPP27007_CGACT_STATE_DEACTIVATED = 0, //!< Context activated.
    AT3GPP27007_CGACT_STATE_ACTIVATED = 1,   //!< Context deactivated.
};
typedef enum At3Gpp27007_CGACT_State_E At3Gpp27007_CGACT_State_T;

/**
 * @brief 3GPP TS 27.007 `AT+CGACT=<state>,<cid>` parameters.
 *
 * @note Even though the CGACT setter command allows to set the state for
 * multiple contexts at a time, we only support changing the state one context
 * at a time. You may of course call the setter multiple times if you want to
 * activate multiple contexts.
 */
struct At3Gpp27007_CGACT_Set_S
{
    At3Gpp27007_CGACT_State_T State; //!< De-/Activate PDP context.
    uint8_t Cid;                     //!< ID of th PDP context to de-/activate.
};
typedef struct At3Gpp27007_CGACT_Set_S At3Gpp27007_CGACT_Set_T;

enum At3Gpp27007_CGPADDR_AddressType_E
{
    AT3GPP27007_CGPADDR_ADDRESSTYPE_IPV4,   //!< IPv4
    AT3GPP27007_CGPADDR_ADDRESSTYPE_IPV6,   //!< IPv6
    AT3GPP27007_CGPADDR_ADDRESSTYPE_INVALID //!< Invalid address
};
typedef enum At3Gpp27007_CGPADDR_AddressType_E At3Gpp27007_CGPADDR_AddressType_T;

struct At3Gpp27007_CGPADDR_Address_S
{
    At3Gpp27007_CGPADDR_AddressType_T Type; //!< Selects which member of #Address is active.
    union {
        uint8_t IPv4[4];  //!< IPv4 representation.
        uint8_t IPv6[16]; //!< IPv6 representation.
    } Address;
};
typedef struct At3Gpp27007_CGPADDR_Address_S At3Gpp27007_CGPADDR_Address_T;

struct At3Gpp27007_CGPADDR_Query_S
{
    /**
     * @brief Context-Id to query.
     */
    uint8_t Cid;
};
typedef struct At3Gpp27007_CGPADDR_Query_S At3Gpp27007_CGPADDR_Query_T;

struct At3Gpp27007_CGPADDR_QueryResponse_S
{
    /**
     * @brief Context-Id queried.
     */
    uint8_t Cid;

    /**
     * @brief Address of the PDP context.
     *
     * @note An IP address of all zeroes (IPv4: 0.0.0.0) and type INVALID means
     * is currently associated with this context.
     */
    At3Gpp27007_CGPADDR_Address_T PdpAddr;
};
typedef struct At3Gpp27007_CGPADDR_QueryResponse_S At3Gpp27007_CGPADDR_QueryResponse_T;

/**
 * @brief 3GPP TS 27.007 `AT+CPIN=<pin>[,<newpin>]` set parameters.
 */
struct At3Gpp27007_CPIN_Set_S
{
    /**
     * @brief PIN to unlock (U)SIM.
     *
     * In case (U)SIM is in PUK-unlock mode, PUK-PIN must be provided as well as
     * #NewPin, to be set as new (U)SIM PIN after unlock.
     */
    const char *Pin;
    /**
     * @brief New PIN to be stored after successful unlock.
     *
     * Parameter will be ignored if set to `NULL` and no PUK-unlock required.
     * Parameter @b must be set to a valid new PIN in case of PUK-unlock.
     */
    const char *NewPin;
};
typedef struct At3Gpp27007_CPIN_Set_S At3Gpp27007_CPIN_Set_T;

/**
 * @brief Possible values for 3GPP TS 27.007 `AT+CPIN?` `<code>` response
 * argument.
 */
enum At3Gpp27007_CPIN_Code_E
{
    AT3GPP27007_CPIN_CODE_READY,         //!< MT is not pending for any password.
    AT3GPP27007_CPIN_CODE_SIM_PIN,       //!< MT is waiting for SIM PIN to be given.
    AT3GPP27007_CPIN_CODE_SIM_PUK,       //!< MT is waiting for SIM PUK to be given.
    AT3GPP27007_CPIN_CODE_PH_SIM_PIN,    //!< MT is waiting for phone-to-SIM card password to be given.
    AT3GPP27007_CPIN_CODE_PH_FSIM_PIN,   //!< MT is waiting phone-to-very first SIM card password to be given.
    AT3GPP27007_CPIN_CODE_PH_FSIM_PUK,   //!< MT is waiting phone-to-very first SIM card unblocking password to be given.
    AT3GPP27007_CPIN_CODE_SIM_PIN2,      //!< MT is waiting SIM PIN2 to be given.
    AT3GPP27007_CPIN_CODE_SIM_PUK2,      //!< MT is waiting SIM PUK2 to be given.
    AT3GPP27007_CPIN_CODE_PH_NET_PIN,    //!< MT is waiting network personalization password to be given.
    AT3GPP27007_CPIN_CODE_PH_NET_PUK,    //!< MT is waiting network personalization unblocking password to be given.
    AT3GPP27007_CPIN_CODE_PH_NETSUB_PIN, //!< MT is waiting network subset personalization password to be given.
    AT3GPP27007_CPIN_CODE_PH_NETSUB_PUK, //!< MT is waiting network subset personalization unblocking password to be given.
    AT3GPP27007_CPIN_CODE_PH_SP_PIN,     //!< MT is waiting service provider personalization password to be given.
    AT3GPP27007_CPIN_CODE_PH_SP_PUK,     //!< MT is waiting service provider personalization unblocking password to be given.
    AT3GPP27007_CPIN_CODE_PH_CORP_PIN,   //!< MT is waiting corporate personalization password to be given.
    AT3GPP27007_CPIN_CODE_PH_CORP_PUK    //!< MT is waiting corporate personalization unblocking password to be given.
};
typedef enum At3Gpp27007_CPIN_Code_E At3Gpp27007_CPIN_Code_T;

/**
 * @brief 3GPP TS 27.007 `AT+CPIN?` get response arguments.
 */
struct At3Gpp27007_CPIN_GetResponse_S
{
    /**
     * @brief Password-type needed to unlock (U)SIM.
     */
    At3Gpp27007_CPIN_Code_T Code;
};
typedef struct At3Gpp27007_CPIN_GetResponse_S At3Gpp27007_CPIN_GetResponse_T;

/**
 * @brief 3GPP TS 27.007 CFUN `<fun>` parameter used to select the MT
 * functionality level.
 */
enum At3Gpp27007_CFUN_Fun_E
{
    AT3GPP27007_CFUN_FUN_MINIMUM = 0,       //!< Set the MT to minimum functionality (disable rx/tx RF circuits).
    AT3GPP27007_CFUN_FUN_FULL = 1,          //!< Sets the MT to full functionality.
    AT3GPP27007_CFUN_FUN_DISABLETX = 2,     //!< Disables tx RF circuits.
    AT3GPP27007_CFUN_FUN_DISABLERX = 3,     //!< Disables rx RF circuits.
    AT3GPP27007_CFUN_FUN_DISABLERXTX = 4,   //!< Disables rx/tx RF circuits to establish air-plane-mode.
    AT3GPP27007_CFUN_FUN_RESERVEDSTART = 5, //!< Start of the manufacture specific values. Check with your device manufacturer for available modes.
    /* gap */
    AT3GPP27007_CFUN_FUN_RESERVEDEND = 127,     //!< End of the manufacture specific values.
    AT3GPP27007_CFUN_FUN_FULLSRA = 129,         //!< Sets the MT to full functionality with radio access support according to the setting of +CSRA.
    AT3GPP27007_CFUN_FUN_PREPARESHUTDOWN = 128, //!< Prepare for shutdown.
    AT3GPP27007_CFUN_FUN_INVALID = 255          //!< Invalid `<fun>` value.
};
typedef enum At3Gpp27007_CFUN_Fun_E At3Gpp27007_CFUN_Fun_T;

/**
 * @brief 3GPP TS 27.007 CFUN `<rst>` parameter used to control if the MT should
 * perform a reset before switching to the desired `<fun>` mode.
 */
enum At3Gpp27007_CFUN_Rst_E
{
    AT3GPP27007_CFUN_RST_NORESET = 0,  //!< Do not reset MT before switching to the selected `<fun>`.
    AT3GPP27007_CFUN_RST_RESET = 1,    //!< Perform a silent MT reset (with graceful network detach) before switching to `<fun>`.
    AT3GPP27007_CFUN_RST_INVALID = 255 //!< Invalid `<rst>` value.
};
typedef enum At3Gpp27007_CFUN_Rst_E At3Gpp27007_CFUN_Rst_T;

/**
 * @brief 3GPP TS 27.007 CFUN set parameters.
 */
struct At3Gpp27007_CFUN_Set_S
{
    At3Gpp27007_CFUN_Fun_T Fun; //!< ME function mode to enter.
    At3Gpp27007_CFUN_Rst_T Rst; //!< Transition action.
};
typedef struct At3Gpp27007_CFUN_Set_S At3Gpp27007_CFUN_Set_T;

/**
 * @brief 3GPP TS 27.007 CFUN get response.
 */
struct At3Gpp27007_CFUN_GetResponse_S
{
    At3Gpp27007_CFUN_Fun_T Fun; //!< Current ME function mode.
};
typedef struct At3Gpp27007_CFUN_GetResponse_S At3Gpp27007_CFUN_GetResponse_T;

/**
 * @brief 3GPP TS 27.007 CMEE `<n>` parameter.
 */
enum At3Gpp27007_CMEE_N_E
{
    AT3GPP27007_CMEE_N_DISABLED = 0, //!< CME ERROR response disabled.
    AT3GPP27007_CMEE_N_NUMERIC = 1,  //!< CME ERROR response numeric.
    AT3GPP27007_CMEE_N_VERBOSE = 2,  //!< CME ERROR response verbose textual.
    AT3GPP27007_CMEE_N_INVALID = 255 //!< Invalid `<n>` value.
};
typedef enum At3Gpp27007_CMEE_N_E At3Gpp27007_CMEE_N_T;

struct At3Gpp27007_CMEE_SetAndGetResponse_S
{
    At3Gpp27007_CMEE_N_T N; //!< Error reporting mode.
};
typedef struct At3Gpp27007_CMEE_SetAndGetResponse_S At3Gpp27007_CMEE_Set_T, At3Gpp27007_CMEE_GetResponse_T;

/* *** NETWORK COMMANDS ***************************************************** */

/**
 * @brief Set the mode and information content of the CREG (network
 * registration) URC.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[in] set
 * Set parameters for `+CREG`.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_SetCREG(struct AtTransceiver_S *t, const At3Gpp27007_CXREG_Set_T *set);

/**
 * @brief Set the mode and information content of the CGREG (GPRS network
 * registration) URC.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[in] set
 * Set parameters for `+CGREG`.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_SetCGREG(struct AtTransceiver_S *t, const At3Gpp27007_CXREG_Set_T *set);

/**
 * @brief Set the mode and information content of the CEREG (EPS network
 * registration) URC.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[in] set
 * Set parameters for `+CEREG`.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_SetCEREG(struct AtTransceiver_S *t, const At3Gpp27007_CXREG_Set_T *set);

/**
 * @brief Get the mode of the CREG (network registration) URC.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[out] resp
 * On entry, must point to allocated instance of the structure. On exit, will
 * contain response arguments obtained from `AT+CREG?` get command.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_GetCREG(struct AtTransceiver_S *t, At3Gpp27007_CREG_GetResponse_T *resp);

/**
 * @brief Get the mode of the CGREG (GPRS network registration) URC.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[out] resp
 * On entry, must point to allocated instance of the structure. On exit, will
 * contain response arguments obtained from `AT+CGREG?` get command.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_GetCGREG(struct AtTransceiver_S *t, At3Gpp27007_CGREG_GetResponse_T *resp);

/**
 * @brief Get the mode of the CEREG (EPS network registration) URC.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[out] resp
 * On entry, must point to allocated instance of the structure. On exit, will
 * contain response arguments obtained from `AT+CEREG?` get command.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_GetCEREG(struct AtTransceiver_S *t, At3Gpp27007_CEREG_GetResponse_T *resp);

/**
 * @brief Set the mode of the CMEE (mobile termination error) .
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[in] set
 * Set parameter for `+CMEE ERROR` formatting.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_SetCMEE(struct AtTransceiver_S *t, const At3Gpp27007_CMEE_Set_T *set);

/**
 * @brief Set the network operator selection criteria for GSM/UMTS/EPS/etc.
 * The modem will rely on these to choose an operator to register to.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[in] set
 * Valid structure pointer containing the network operator criteria details.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_SetCOPS(struct AtTransceiver_S *t, const At3Gpp27007_COPS_Set_T *set);

/**
 * @brief Set the connection parameters for a data-context.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[in] set
 * Valid structure pointer containing the context definition parameters.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_SetCGDCONT(struct AtTransceiver_S *t, const At3Gpp27007_CGDCONT_Set_T *set);

/**
 * @brief Activate or deactivate a specified data-context.
 *
 * @note If the specified data-context is already activated or deactivated, no
 * action is taken.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[in] set
 * Valid structure pointer containing the details of which context to
 * activate/deactivate.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_SetCGACT(struct AtTransceiver_S *t, const At3Gpp27007_CGACT_Set_T *set);

/**
 * @brief Show the PDP address for the specified Context-Id.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[in] query
 * Valid structure pointer to initiate the 'Show Address' query. The
 * addresses will be written into the response structure.
 * @param[out] resp
 * Valid structure to hold the response data of the `CGPADDR` query command.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_QueryCGPADDR(struct AtTransceiver_S *t, const At3Gpp27007_CGPADDR_Query_T *query, At3Gpp27007_CGPADDR_QueryResponse_T *resp);

/* *** SIM COMMANDS ********************************************************* */

/**
 * @brief Enter the PIN/PUK to unlock the SIM card.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[in] set
 * (U)SIM unlock parameters.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_SetCPIN(struct AtTransceiver_S *t, const At3Gpp27007_CPIN_Set_T *set);

/**
 * @brief Get (U)SIM lock state.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[out] resp
 * Valid structure to hold the response arguments obtained from `CPIN` get
 * command.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_GetCPIN(struct AtTransceiver_S *t, At3Gpp27007_CPIN_GetResponse_T *resp);

/* *** TE-TA INTERFACE COMMANDS ********************************************* */

/**
 * @brief Send out the AT no-operation command (`AT<S3><S4>`) and wait for
 * response.
 *
 * Can be used to probe the AT device for responsiveness.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_ExecuteAT(struct AtTransceiver_S *t);

/**
 * @brief Enable or disable command echoing of the DCE (modem).
 *
 * Refer to ITU-T Recommendation V.250 (07/2003).
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[in] enableEcho
 * True if echoing should be enabled, false otherwise.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_ExecuteATE(struct AtTransceiver_S *t, bool enableEcho);

/* *** POWER CONTROL COMMANDS *********************************************** */

/**
 * @brief Set the MT to a desired functionality level or perform a MT reset.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[in] set
 * Valid structure pointer that contains the desired functionality level and
 * optional reset behavior.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_SetCFUN(struct AtTransceiver_S *t, const At3Gpp27007_CFUN_Set_T *set);

/**
 * @brief Query the MT functionality state.
 *
 * @param[in,out] t
 * Prepared transceiver instance to send and receive on.
 * @param[out] resp
 * On entry, must point to allocated instance of the structure. On exit, will
 * contain response arguments obtained from `+CFUN` get command.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_GetCFUN(struct AtTransceiver_S *t, At3Gpp27007_CFUN_GetResponse_T *resp);

/* *** URC HANDLERS ********************************************************* */

/**
 * @brief Parse CREG URC arguments.
 *
 * URC handling implies the AT-cmd was already consumed and the given
 * transceiver starts with reading the first argument.
 *
 * @param[in,out] t
 * Prepared transceiver instance to receive on.
 * @param[out] resp
 * On entry, must point to allocated instance of the structure. On exit, will
 * contain response arguments obtained from from `+CREG` URC.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_UrcCREG(struct AtTransceiver_S *t, At3Gpp27007_CREG_UrcResponse_T *resp);

/**
 * @brief Parse CGREG URC arguments.
 *
 * URC handling implies the AT-cmd was already consumed and the given
 * transceiver starts with reading the first argument.
 *
 * @param[in,out] t
 * Prepared transceiver instance to receive on.
 * @param[out] resp
 * On entry, must point to allocated instance of the structure. On exit, will
 * contain response arguments obtained from `+CGREG` URC.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_UrcCGREG(struct AtTransceiver_S *t, At3Gpp27007_CGREG_UrcResponse_T *resp);

/**
 * @brief Parse CEREG URC arguments.
 *
 * URC handling implies the AT-cmd was already consumed and the given
 * transceiver starts with reading the first argument.
 *
 * @param[in,out] t
 * Prepared transceiver instance to receive on.
 * @param[out] resp
 * On entry, must point to allocated instance of the structure. On exit, will
 * contain response arguments obtained from `+CEREG` URC.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T At3Gpp27007_UrcCEREG(struct AtTransceiver_S *t, At3Gpp27007_CEREG_UrcResponse_T *resp);

#endif /* AT3GPP27007_H_ */

/** @} */
