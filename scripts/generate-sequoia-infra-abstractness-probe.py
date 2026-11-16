#!/usr/bin/env python3
"""Generate a local compile-only concrete-subclass probe from supplied Sequoia headers.

The input headers are never copied into the repository. The generated output is
written only to the caller-selected path and should normally live in a temp dir.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

EXPECTED_INFRA_METHOD_ORDER = (
    "getCHANNEL", "getPOWERSAVE", "getTXPOWER", "getRATE", "getOP_MODE", "getRSSI",
    "getSUPPORTED_CHANNELS", "getGUARD_INTERVAL", "getMCS", "getPOWER_DEBUG_INFO",
    "getHT_CAPABILITY", "getMCS_VHT", "getCHANNELS_INFO", "getVHT_CAPABILITY",
    "getROAM_PROFILE", "getCHIP_COUNTER_STATS", "getDBG_GUARD_TIME_PARAMS",
    "getLEAKY_AP_STATS_MODE", "getCOUNTRY_CHANNELS", "getPRIVATE_MAC",
    "getRANGING_ENABLE", "getRANGING_START", "getAWDL_RSDB_CAPS", "getTKO_PARAMS",
    "getTKO_DUMP", "getHW_SUPPORTED_CHANNELS", "getBTCOEX_PROFILE",
    "getBTCOEX_PROFILE_ACTIVE", "getTRAP_INFO", "getTHERMAL_INDEX", "getMAX_NSS_FOR_AP",
    "getBTCOEX_2G_CHAIN_DISABLE", "getPOWER_BUDGET", "getOFFLOAD_TCPKA_ENABLE",
    "getRANGING_CAPS", "getLQM_CONFIG", "getTRAP_CRASHTRACER_MINI_DUMP", "getBEACON_INFO",
    "getCHIP_POWER_RANGE", "getNSS", "getHW_ADDR", "getCHIP_DIAGS", "getHP2P_CTRL",
    "getBSS_BLACKLIST", "getTXRX_CHAIN_INFO", "getMIMO_STATUS", "getCUR_PMK",
    "getDYNSAR_DETAIL", "getCOUNTRY_CHANNELS_INFO", "getLQM_SUMMARY",
    "getSLOW_WIFI_FEATURE_ENABLED", "getTIMESYNC_INFO", "getSENSING_DATA",
    "getWCL_FW_HOT_CHANNELS", "getWCL_LOW_LATENCY_INFO", "getWCL_BSS_INFO",
    "getWCL_TRAFFIC_COUNTERS", "getWCL_GET_TX_BLANKING_STATUS", "getHE_COUNTERS",
    "getWCL_CHANNELS_INFO", "getRSN_XE", "getSIB_COEX_STATUS", "getWIFI_BT_5G_POLICY",
    "getWCL_EXTENDED_BSS_INFO", "getWCL_LOW_LATENCY_INFO_STATS", "getWCL_BGSCAN_CACHE_RESULT",
    "getWCL_WNM_OFFLOAD", "getWIFI_NOISE_PER_ANT", "getFW_CLOCK_INFO", "getTIMESYNC_STATS",
    "getSYSTEM_SLEEP_CONFIG", "setCIPHER_KEY", "setCHANNEL", "setPOWERSAVE", "setTXPOWER",
    "setRATE", "setIBSS_MODE", "setAP_MODE", "setIE", "setWOW_TEST", "setCLEAR_PMKSA_CACHE",
    "setVIRTUAL_IF_CREATE", "setHT_CAPABILITY", "setOFFLOAD_ARP", "setOFFLOAD_NDP",
    "setGAS_REQ", "setVHT_CAPABILITY", "setROAM_PROFILE", "setDBG_GUARD_TIME_PARAMS",
    "setLEAKY_AP_STATS_MODE", "setPRIVATE_MAC", "setRESET_CHIP", "setCRASH",
    "setRANGING_ENABLE", "setRANGING_START", "setRANGING_AUTHENTICATE", "setTKO_PARAMS",
    "setBTCOEX_PROFILE", "setBTCOEX_PROFILE_ACTIVE", "setTHERMAL_INDEX",
    "setBTCOEX_2G_CHAIN_DISABLE", "setPOWER_BUDGET", "setOFFLOAD_TCPKA_ENABLE",
    "setLQM_CONFIG", "setDYNAMIC_RSSI_WINDOW_CONFIG", "setUSB_HOST_NOTIFICATION",
    "setHP2P_CTRL", "setBSS_BLACKLIST", "setSET_PROPERTY", "setROAM_CACHE_UPDATE",
    "setPM_MODE", "setSET_WIFI_ASSERTION_STATE", "setREALTIME_QOS_MSCS", "setSENSING_ENABLE",
    "setSENSING_DISABLE", "set6G_MODE", "setWCL_LEAVE_NETWORK", "setWCL_REASSOC",
    "setWCL_SET_ROAM_LOCK", "setWCL_LEGACY_ROAM_PROFILE_CONFIG", "setWCL_ROAM_PROFILE_CONFIG",
    "setWCL_ROAM_USER_CACHE", "setWCL_SCAN_ABORT", "setWCL_REAL_TIME_MODE", "setWCL_ARP_MODE",
    "setWCL_JOIN_ABORT", "setWCL_TRIGGER_CC", "setWCL_SCAN_REQ", "setWCL_ASSOCIATE",
    "setWCL_QOS_PARAMS", "setWCL_LINK_UP_DONE", "setWCL_SET_SCAN_HOME_AWAY_TIME",
    "setVOICE_IND_STATE", "setWCL_LINK_STATE_UPDATE", "setRSN_XE", "setWIFI_BT_5G_POLICY",
    "setWCL_ULOFDMA_STATE", "setWCL_ACTION_FRAME", "setGAS_ABORT", "setOS_FEATURE_FLAGS",
    "setDHCP_RENEWAL_DATA", "setBATTERY_POWERSAVE_CONFIG", "setMIMO_CONFIG",
    "setWCL_CONFIG_BG_MOTIONPROFILE", "setWCL_CONFIG_BG_NETWORK", "setWCL_CONFIG_BGSCAN",
    "setWCL_CONFIG_BG_PARAMS", "setPOWER_PROFILE", "setHEARTBEAT", "setINTERFACE_SETTING",
    "setBYPASS_TX_POWER_CAP", "setFACETIME_WIFICALLING_PARAMS", "setIPV4_PARAMS",
    "setWCL_WNM_OPS", "setWCL_WNM_OFFLOAD", "setWCL_LIMITED_AGGREGATION", "setWCL_BCN_MUTE_CONFIG",
    "setEAP_FILTER_CONFIG", "setWOW_LOW_POWER_MODE", "setDUAL_POWER_MODE",
    "setWCL_UPDATE_FAST_LANE", "setWCL_ASSOCIATED_SLEEP", "setCONGESTION_CTRL_IND",
    "setSTAND_ALONE_MODE_STATE", "setIPV6_PARAMS", "setINFRA_ENUMERATED", "setLMTPC_CONFIG",
    "setTRAFFIC_ENG_PARAMS", "setLE_SCAN_PARAM", "setTIMESYNC_GPIO", "setHOST_CLOCK_INFO",
    "setFW_CLOCK_SOURCE", "setTIMESYNC_TX_POLICY", "setTIMESYNC_RX_POLICY", "setTIMESTAMPING_EN",
    "setWCL_SOI_CONFIG", "setMWS_TIME_SHARING_WIFI_ENH", "setMWS_WIFI_TYPE_7_BITMAP_WIFI_ENH",
    "setMWS_COEX_BITMAP_WIFI_ENH", "setMWS_DISABLE_OCL_BITMAP_WIFI_ENH",
    "setMWS_RFEM_CONFIG_WIFI_ENH", "setMWS_ASSOC_PROTECTION_BITMAP_WIFI_ENH",
    "setMWS_SCAN_FREQ_WIFI_ENH", "setMWS_SCAN_FREQ_MODE_WIFI_ENH",
    "setMWS_CONDITION_ID_BITMAP_WIFI_ENH", "setMWS_ANTENNA_SELECTION_WIFI_ENH",
    "setNDD_REQ", "setDBRG_ENTROPY", "setSDB_ENABLE", "setMWS_ACCESSORY_POWER_LIMIT_WIFI_ENH",
)


def strip_comments(source: str) -> str:
    # Preserve newlines so diagnostics and declaration boundaries remain readable.
    source = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


def class_body(source: str, class_name: str) -> str:
    match = re.search(r"\bclass\s+" + re.escape(class_name) + r"\b[^\{]*\{", source)
    if not match:
        raise ValueError(f"class {class_name} not found")
    start = match.end()
    depth = 1
    for index in range(start, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start:index]
    raise ValueError(f"unterminated class {class_name}")


def pure_virtuals(header: Path, class_name: str) -> list[str]:
    source = strip_comments(header.read_text(encoding="utf-8"))
    body = class_body(source, class_name)
    declarations = re.findall(r"\bvirtual\s+([^;{}]+?)\s*=\s*0\s*;", body, flags=re.S)
    result = []
    for declaration in declarations:
        declaration = " ".join(declaration.split()).strip()
        if not declaration.startswith("IOReturn "):
            raise ValueError(
                f"{class_name} has unsupported pure-virtual return/signature: {declaration}"
            )
        result.append(declaration)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--infra-header", required=True, type=Path)
    parser.add_argument("--skywalk-header", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--expected-infra", type=int, default=190)
    parser.add_argument("--expected-skywalk", type=int, default=1)
    args = parser.parse_args()

    try:
        infra = pure_virtuals(args.infra_header, "IO80211InfraProtocol")
        skywalk = pure_virtuals(args.skywalk_header, "IO80211SkywalkInterface")
        if len(infra) != args.expected_infra:
            raise ValueError(
                f"IO80211InfraProtocol pure-virtual count {len(infra)} "
                f"!= expected {args.expected_infra}"
            )
        if len(skywalk) != args.expected_skywalk:
            raise ValueError(
                f"IO80211SkywalkInterface pure-virtual count {len(skywalk)} "
                f"!= expected {args.expected_skywalk}"
            )

        infra_names = [re.search(r"\b([A-Za-z_]\w*)\s*\(", item).group(1)
                       for item in infra]
        skywalk_names = [re.search(r"\b([A-Za-z_]\w*)\s*\(", item).group(1)
                         for item in skywalk]
        if args.expected_infra == 190 and tuple(infra_names) != EXPECTED_INFRA_METHOD_ORDER:
            first = next((i for i, pair in enumerate(zip(infra_names, EXPECTED_INFRA_METHOD_ORDER))
                          if pair[0] != pair[1]), min(len(infra_names), len(EXPECTED_INFRA_METHOD_ORDER)))
            raise ValueError(
                "IO80211InfraProtocol pure-virtual method order differs from the audited "
                f"Sequoia ABI at slot {first}: "
                f"actual={infra_names[first:first + 1]}, "
                f"expected={list(EXPECTED_INFRA_METHOD_ORDER[first:first + 1])}"
            )
        if args.expected_skywalk == 1 and skywalk_names != ["setMacAddress"]:
            raise ValueError(
                "IO80211SkywalkInterface pure-virtual method differs from audited ABI: "
                f"{skywalk_names}"
            )

        declarations = skywalk + infra
        names = []
        methods = []
        for declaration in declarations:
            match = re.search(r"\b([A-Za-z_]\w*)\s*\(", declaration)
            if not match:
                raise ValueError(f"cannot parse pure-virtual method name: {declaration}")
            names.append(match.group(1))
            methods.append(
                "    virtual " + declaration + " override { return kIOReturnUnsupported; }"
            )
        if len(set(names)) != len(names):
            raise ValueError("duplicate pure-virtual method names; refusing ambiguous probe")

        output = [
            "// Generated locally from caller-supplied Sequoia headers; do not commit.",
            "#pragma once",
            "#include <type_traits>",
            "#include <IOKit/IOService.h>",
            "#include <net/ethernet.h>",
            "// Load the public Apple80211 ABI first: it defines IO80211LinkState,",
            "// apple80211_scan_data/channel types, ether_addr and ether_header.",
            '#include "apple_private_spi.h"',
            '#include "apple80211_ioctl.h"',
            "// nicproxy_info_s is used only through pointers in these declarations.",
            "struct nicproxy_info_s;",
            "typedef struct nicproxy_info_s nicproxy_info_s;",
            "// Sequoia InfraProtocol uses this opaque statistics record by pointer.",
            "struct apple80211_chip_stats;",
            "// InfraInterface derives from SkywalkInterface; establish the base first.",
            '#include "IO80211SkywalkInterface.h"',
            '#include "IO80211InfraProtocol.h"',
            "",
            "class QcaAirportInfraProtocolCompileProbe : public IO80211InfraProtocol {",
            "public:",
            *methods,
            "};",
            "",
            "static_assert(!std::is_abstract<QcaAirportInfraProtocolCompileProbe>::value,",
            '              "Sequoia infra protocol subclass still has pure virtual methods");',
            "",
        ]
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text("\n".join(output), encoding="utf-8")
        print(
            f"generated {len(infra)} infra + {len(skywalk)} inherited pure-virtual "
            f"stubs at {args.output}"
        )
        return 0
    except (OSError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
