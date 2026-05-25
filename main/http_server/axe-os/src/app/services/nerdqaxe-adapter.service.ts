/**
 * NerdQAxe API Adapter
 *
 * Maps NerdQAxe firmware API responses to the field names/types
 * the LottoAxe OS Angular UI expects (originally built for Bitaxe ESP-Miner).
 *
 * The LottoAxe UI uses the SystemInfo interface from generated/models.
 * The NerdQAxe firmware returns different field names, types, and structures.
 * This adapter normalizes the response so the UI works without changes.
 */

import { Injectable } from '@angular/core';
import { SystemInfo } from '../generated/models';

@Injectable({
  providedIn: 'root'
})
export class NerdQAxeAdapterService {

  /**
   * Maps a raw NerdQAxe /api/system/info response to the SystemInfo interface
   * the LottoAxe UI components expect.
   */
  mapSystemInfo(raw: any): SystemInfo {
    return {
      // --- Direct mappings (same field names) ---
      power: raw.power ?? 0,
      current: raw.current ?? 0,
      voltage: raw.voltage ?? 0,
      hashRate: raw.hashRate ?? 0,
      hashRate_1m: raw.hashRate_1m ?? 0,
      hashRate_10m: raw.hashRate_10m ?? 0,
      hashRate_1h: raw.hashRate_1h ?? 0,
      coreVoltage: raw.coreVoltage ?? 0,
      coreVoltageActual: raw.coreVoltageActual ?? 0,
      maxPower: raw.maxPower ?? 0,
      bestDiff: raw.bestDiff ?? 0,
      bestSessionDiff: raw.bestSessionDiff ?? 0,
      sharesAccepted: raw.sharesAccepted ?? 0,
      sharesRejected: raw.sharesRejected ?? 0,
      uptimeSeconds: raw.uptimeSeconds ?? 0,
      poolDifficulty: raw.poolDifficulty ?? 0,
      ASICModel: raw.ASICModel ?? 'Unknown',
      ssid: raw.ssid ?? '',
      macAddr: raw.macAddr ?? '',
      hostname: raw.hostname ?? '',
      wifiRSSI: raw.wifiRSSI ?? 0,
      stratumURL: raw.stratumURL ?? '',
      stratumPort: raw.stratumPort ?? 0,
      stratumUser: raw.stratumUser ?? '',
      fallbackStratumURL: raw.fallbackStratumURL ?? '',
      fallbackStratumPort: raw.fallbackStratumPort ?? 0,
      fallbackStratumUser: raw.fallbackStratumUser ?? '',
      smallCoreCount: raw.smallCoreCount ?? 0,
      runningPartition: raw.runningPartition ?? '',
      invertscreen: raw.invertscreen ?? 0,
      manualFanSpeed: raw.manualFanSpeed ?? 0,
      fanspeed: raw.fanspeed ?? 0,
      fanrpm: raw.fanrpm ?? 0,
      vrTemp: raw.vrTemp ?? 0,
      temp: raw.temp ?? 0,
      version: raw.version ?? '',
      frequency: raw.frequency ?? 0,
      networkDifficulty: raw.networkDifficulty ?? 0,
      blockHeight: raw.blockHeight ?? 0,

      // --- Renamed fields (NerdQAxe uses different names) ---
      ipv4: raw.hostip ?? raw.ipv4 ?? '',
      fan2rpm: raw.fanrpm2 ?? raw.fan2rpm ?? 0,
      resetReason: raw.lastResetReason ?? raw.resetReason ?? '',
      freeHeap: raw.freeHeap ?? 0,
      freeHeapInternal: raw.freeHeapInt ?? raw.freeHeapInternal ?? 0,
      freeHeapSpiram: raw.freeHeap ?? raw.freeHeapSpiram ?? 0,
      stratumSuggestedDifficulty: raw.stratumDifficulty ?? raw.stratumSuggestedDifficulty ?? 0,
      fallbackStratumSuggestedDifficulty: raw.fallbackStratumDifficulty ?? raw.fallbackStratumSuggestedDifficulty ?? 0,

      // --- Type conversions (NerdQAxe uses int enums, LottoAxe uses strings) ---
      stratumProtocol: this.mapProtocol(raw.stratumProtocol),
      fallbackStratumProtocol: this.mapProtocol(raw.fallbackStratumProtocol),
      stratumTLS: !!raw.stratumTLS,
      fallbackStratumTLS: !!raw.fallbackStratumTLS,
      stratumExtranonceSubscribe: !!(raw.stratumEnonceSubscribe ?? raw.stratumExtranonceSubscribe),
      fallbackStratumExtranonceSubscribe: !!(raw.fallbackStratumEnonceSubscribe ?? raw.fallbackStratumExtranonceSubscribe),
      stratumV2AuthorityPubkey: raw.sv2AuthorityPubkey ?? raw.stratumV2AuthorityPubkey ?? '',
      stratumV2ChannelType: this.mapChannelType(raw.sv2ChannelType ?? raw.stratumV2ChannelType),
      fallbackStratumV2AuthorityPubkey: raw.fallbackSv2AuthorityPubkey ?? raw.fallbackStratumV2AuthorityPubkey ?? '',
      fallbackStratumV2ChannelType: this.mapChannelType(raw.fallbackSv2ChannelType ?? raw.fallbackStratumV2ChannelType),

      // --- Fan control mapping ---
      // NerdQAxe uses per-channel fans[] array; LottoAxe uses flat fields
      autofanspeed: raw.autofanspeed ?? 0,
      temptarget: raw.pidTargetTemp ?? raw.temptarget ?? 60,
      minFanSpeed: this.extractMinFanSpeed(raw),

      // --- Fields that don't exist on NerdQAxe (safe defaults) ---
      temp2: raw.asicTemps?.[1] ?? raw.temp2 ?? 0,
      nominalVoltage: raw.nominalVoltage ?? 5,
      expectedHashrate: raw.expectedHashrate ?? 0,
      errorPercentage: raw.errorPercentage ?? 0,
      cpuUsage: raw.cpuUsage ?? 0,
      responseTime: raw.responseTime ?? 0,
      ipv6: raw.ipv6 ?? '',
      wifiStatus: raw.wifiStatus ?? 'Connected',
      apEnabled: raw.apEnabled ?? 0,
      axeOSVersion: raw.version ?? '',
      idfVersion: raw.idfVersion ?? '',
      boardVersion: raw.deviceModel ?? raw.boardVersion ?? '',
      display: raw.display ?? '',
      rotation: raw.rotation ?? 0,
      displayTimeout: raw.displayTimeout ?? -1,
      isPSRAMAvailable: raw.isPSRAMAvailable ?? 1,
      overclockEnabled: raw.overclockEnabled ?? 1,
      statsFrequency: raw.statsFrequency ?? 30,
      poolConnectionInfo: raw.poolConnectionInfo ?? '',
      isUsingFallbackStratum: raw.isUsingFallbackStratum ?? 0,
      actualFrequency: raw.frequency ?? raw.actualFrequency ?? 0,
      stratumCert: raw.stratumCert ?? '',
      stratumDecodeCoinbase: raw.stratumDecodeCoinbase ?? raw.coinbaseVerifyMode > 0 ?? false,
      fallbackStratumCert: raw.fallbackStratumCert ?? '',
      fallbackStratumDecodeCoinbase: raw.fallbackStratumDecodeCoinbase ?? false,
      activeProtocolLabel: this.mapProtocol(raw.stratumProtocol),
      sharesRejectedReasons: raw.sharesRejectedReasons ?? [],
      scriptsig: raw.blockHeaders?.[0]?.scriptsig ?? raw.scriptsig ?? '',
      blockSignals: raw.blockSignals ?? [],
      overheat_mode: raw.overheat_mode ?? 0,
      miningPaused: raw.miningPaused ?? raw.shutdown ?? false,
      blockFound: raw.foundBlocks ?? raw.totalFoundBlocks ?? raw.blockFound ?? 0,
      showNewBlock: raw.showNewBlock ?? false,
      coinbaseOutputs: raw.coinbaseOutputs ?? [],
      coinbaseValueTotalSatoshis: raw.coinbaseValueTotalSatoshis ?? 0,
      coinbaseValueUserSatoshis: raw.coinbaseValueUserSatoshis ?? 0,

      // --- Hashrate monitor (map NerdQAxe asicTemps to the LottoAxe format) ---
      hashrateMonitor: raw.hashrateMonitor ?? {
        asics: (raw.asicTemps ?? []).map((t: number) => ({
          total: raw.hashRate ?? 0,
          domains: [],
          errorCount: 0,
        })),
        hashrate: raw.hashRate ?? 0,
      },

      // --- NerdQAxe-specific fields (pass through for components that use them) ---
      ...(raw.deviceModel && { deviceModel: raw.deviceModel }),
      ...(raw.asicCount && { asicCount: raw.asicCount }),
      ...(raw.asicTemps && { asicTemps: raw.asicTemps }),
      ...(raw.fans && { fans: raw.fans }),
      ...(raw.can && { can: raw.can }),
      ...(raw.stratum && { stratumManager: raw.stratum }),
      ...(raw.otp !== undefined && { otp: raw.otp }),
    } as any;
  }

  /**
   * Maps NerdQAxe settings payload FROM LottoAxe field names TO NerdQAxe field names
   * for PATCH /api/system requests
   */
  mapSettingsToNerdQAxe(settings: any): any {
    const mapped: any = { ...settings };

    // Rename fields the NerdQAxe firmware expects
    if ('stratumSuggestedDifficulty' in mapped) {
      mapped.stratumDifficulty = mapped.stratumSuggestedDifficulty;
      delete mapped.stratumSuggestedDifficulty;
    }
    if ('stratumExtranonceSubscribe' in mapped) {
      mapped.stratumEnonceSubscribe = mapped.stratumExtranonceSubscribe;
      delete mapped.stratumExtranonceSubscribe;
    }
    if ('fallbackStratumExtranonceSubscribe' in mapped) {
      mapped.fallbackStratumEnonceSubscribe = mapped.fallbackStratumExtranonceSubscribe;
      delete mapped.fallbackStratumExtranonceSubscribe;
    }
    if ('stratumV2AuthorityPubkey' in mapped) {
      mapped.sv2AuthorityPubkey = mapped.stratumV2AuthorityPubkey;
      delete mapped.stratumV2AuthorityPubkey;
    }
    if ('fallbackStratumV2AuthorityPubkey' in mapped) {
      mapped.fallbackSv2AuthorityPubkey = mapped.fallbackStratumV2AuthorityPubkey;
      delete mapped.fallbackStratumV2AuthorityPubkey;
    }
    if ('stratumV2ChannelType' in mapped) {
      mapped.sv2ChannelType = this.reverseMapChannelType(mapped.stratumV2ChannelType);
      delete mapped.stratumV2ChannelType;
    }
    if ('fallbackStratumV2ChannelType' in mapped) {
      mapped.fallbackSv2ChannelType = this.reverseMapChannelType(mapped.fallbackStratumV2ChannelType);
      delete mapped.fallbackStratumV2ChannelType;
    }

    // Protocol: string → int
    if ('stratumProtocol' in mapped && typeof mapped.stratumProtocol === 'string') {
      mapped.stratumProtocol = mapped.stratumProtocol === 'SV2' ? 1 : 0;
    }
    if ('fallbackStratumProtocol' in mapped && typeof mapped.fallbackStratumProtocol === 'string') {
      mapped.fallbackStratumProtocol = mapped.fallbackStratumProtocol === 'SV2' ? 1 : 0;
    }

    // Remove fields NerdQAxe doesn't understand
    const ignoreFields = [
      'temptarget', 'minFanSpeed', 'statsFrequency', 'display',
      'displayTimeout', 'rotation', 'overclockEnabled'
    ];
    for (const field of ignoreFields) {
      delete mapped[field];
    }

    return mapped;
  }

  // --- Private helpers ---

  private mapProtocol(val: any): 'SV1' | 'SV2' {
    if (typeof val === 'string') return val as any;
    return val === 1 ? 'SV2' : 'SV1';
  }

  private mapChannelType(val: any): 'standard' | 'extended' | undefined {
    if (typeof val === 'string') return val as any;
    if (val === 0) return 'standard';
    if (val === 1) return 'extended';
    return undefined;
  }

  private reverseMapChannelType(val: any): number {
    if (val === 'extended') return 1;
    return 0;
  }

  private extractMinFanSpeed(raw: any): number {
    // NerdQAxe stores per-fan config in fans[] array
    if (raw.fans?.length > 0 && raw.fans[0].manualSpeed !== undefined) {
      return raw.fans[0].manualSpeed;
    }
    return raw.minFanSpeed ?? 25;
  }
}
