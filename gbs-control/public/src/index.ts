/** STRUCTS */
interface Struct {
  name: string;
  type: "byte" | "string";
  size: number;
}

interface StructDescriptors {
  [key: string]: Struct[];
}

const Structs: StructDescriptors = {
  slots: [
    // --- ORIGINAL GBS-CONTROL ---
    { name: "name", type: "string", size: 25 },
    { name: "presetID", type: "byte", size: 1 },
    { name: "scanlines", type: "byte", size: 1 },
    { name: "scanlinesStrength", type: "byte", size: 1 },
    { name: "slot", type: "byte", size: 1 },
    { name: "wantVdsLineFilter", type: "byte", size: 1 },
    { name: "wantStepResponse", type: "byte", size: 1 },
    { name: "wantPeaking", type: "byte", size: 1 },
    // --- PRO: GBS Processing options ---
    { name: "wantFullHeight", type: "byte", size: 1 },
    { name: "deintMode", type: "byte", size: 1 },
    { name: "enableFrameTimeLock", type: "byte", size: 1 },
    { name: "frameTimeLockMethod", type: "byte", size: 1 },
    { name: "PalForce60", type: "byte", size: 1 },
    { name: "enableAutoGain", type: "byte", size: 1 },
    { name: "wantSharpness", type: "byte", size: 1 },
    // --- PRO: GBS Color balance ---
    { name: "gbsColorR", type: "byte", size: 1 },
    { name: "gbsColorG", type: "byte", size: 1 },
    { name: "gbsColorB", type: "byte", size: 1 },
    // --- PRO: ADV7280 settings ---
    { name: "advSmooth", type: "byte", size: 1 },
    { name: "advI2P", type: "byte", size: 1 },
    { name: "advBrightness", type: "byte", size: 1 },
    { name: "advContrast", type: "byte", size: 1 },
    { name: "advSaturation", type: "byte", size: 1 },
    { name: "advACE", type: "byte", size: 1 },
    // --- PRO: ADV7280 ACE Parameters ---
    { name: "advACELumaGain", type: "byte", size: 1 },      // 0-31, default 13
    { name: "advACEChromaGain", type: "byte", size: 1 },    // 0-15, default 8
    { name: "advACEChromaMax", type: "byte", size: 1 },     // 0-15, default 8
    { name: "advACEGammaGain", type: "byte", size: 1 },     // 0-15, default 8
    { name: "advACEResponseSpeed", type: "byte", size: 1 }, // 0-15, default 15
    // --- PRO: ADV7280 Video Filter Parameters ---
    { name: "advFilterYShaping", type: "byte", size: 1 },   // 0-30, default 1
    { name: "advFilterCShaping", type: "byte", size: 1 },   // 0-7, default 0
    { name: "advFilterWYShaping", type: "byte", size: 1 },  // 2-19, default 9 (SVHS-8)
    { name: "advFilterWYOverride", type: "byte", size: 1 }, // 0-1, default 1 (Manual)
    { name: "advFilterCombNTSC", type: "byte", size: 1 },   // 0-3, default 0
    { name: "advFilterCombPAL", type: "byte", size: 1 },    // 0-3, default 1
    // --- PRO: ADV7280 Comb Control Parameters ---
    { name: "advCombLumaModeNTSC", type: "byte", size: 1 },   // 0,4-7, default 0
    { name: "advCombChromaModeNTSC", type: "byte", size: 1 }, // 0,4-7, default 0
    { name: "advCombChromaTapsNTSC", type: "byte", size: 1 }, // 0-3, default 2
    { name: "advCombLumaModePAL", type: "byte", size: 1 },    // 0,4-7, default 0
    { name: "advCombChromaModePAL", type: "byte", size: 1 },  // 0,4-7, default 0
    { name: "advCombChromaTapsPAL", type: "byte", size: 1 },  // 0-3, default 3
    // --- HDMI Limited Range ---
    { name: "hdmiLimitedRange", type: "byte", size: 1 },    // 0=Off, 1=HD, 2=SD, 3=All
    // --- PRO: ADV7280 Hue (added in v2.3.0) ---
    { name: "advHue", type: "byte", size: 1 },              // 0-255 (default 128 = 0°)
    // --- Reserved for future expansion ---
    { name: "reserved", type: "byte", size: 61 },
  ],
};

// =====================================================================
// Video Filter Name Constants
// =====================================================================

// Y Filter names for Composite (YSFM register 0x17) - 31 values (0-30)
const yFilterNamesAV = [
  "AutoWide", "AutoNarrow",
  "SVHS-1", "SVHS-2", "SVHS-3", "SVHS-4", "SVHS-5", "SVHS-6",
  "SVHS-7", "SVHS-8", "SVHS-9", "SVHS-10", "SVHS-11", "SVHS-12",
  "SVHS-13", "SVHS-14", "SVHS-15", "SVHS-16", "SVHS-17", "SVHS-18",
  "PAL-NN1", "PAL-NN2", "PAL-NN3", "PAL-WN1", "PAL-WN2",
  "NTSC-NN1", "NTSC-NN2", "NTSC-NN3", "NTSC-WN1", "NTSC-WN2", "NTSC-WN3"
];

// WY Filter names for S-Video (WYSFM register 0x18) - 18 values (raw 2-19, array index 0-17)
const yFilterNamesSV = [
  "SVHS-1", "SVHS-2", "SVHS-3", "SVHS-4", "SVHS-5", "SVHS-6",
  "SVHS-7", "SVHS-8", "SVHS-9", "SVHS-10", "SVHS-11", "SVHS-12",
  "SVHS-13", "SVHS-14", "SVHS-15", "SVHS-16", "SVHS-17", "SVHS-18"
];

// C Filter names (CSFM register 0x18) - 8 values (0-7)
const cFilterNames = [
  "Auto1.5M", "Auto2.2M", "SH1", "SH2", "SH3", "SH4", "SH5", "Wideband"
];

// Comb Filter bandwidth names - 4 values (0-3)
const combNames = ["Narrow", "Medium", "Wide", "Widest"];

const StructParser = {
  pos: 0,
  parseStructArray(
    buff: ArrayBuffer,
    structsDescriptors: StructDescriptors,
    struct: string
  ) {
    const currentStruct = structsDescriptors[struct];

    this.pos = 0;
    buff = new Uint8Array(buff);

    if (currentStruct) {
      const structSize = StructParser.getSize(structsDescriptors, struct);

      return [...Array(buff.byteLength / structSize)].map(() => {
        return currentStruct.reduce((acc, structItem) => {
          acc[structItem.name] = this.getValue(buff, structItem);
          return acc;
        }, {});
      });
    }

    return null;
  },
  getValue(buff: any[], structItem: { type: "byte" | "string"; size: number }) {
    switch (structItem.type) {
      case "byte":
        const byteValue = buff[this.pos];
        this.pos += structItem.size;  // Skip all bytes (handles reserved fields)
        return byteValue;

      case "string":
        const currentPos = this.pos;
        this.pos += structItem.size;

        return [...Array(structItem.size)]
          .map(() => " ")
          .map((_char, index) => {
            if (buff[currentPos + index] > 31) {
              return String.fromCharCode(buff[currentPos + index]);
            }
            return "";
          })
          .join("")
          .trim();
    }
  },
  getSize(structsDescriptors: StructDescriptors, struct: string) {
    const currentStruct = structsDescriptors[struct];
    return currentStruct.reduce((acc, prop) => {
      acc += prop.size;
      return acc;
    }, 0);
  },
};

/* GBSControl Global Object*/
const GBSControl = {
  buttonMapping: {
    1: "button1280x960",
    2: "button1280x1024",
    3: "button1280x720",
    4: "button720x480",
    5: "button1920x1080",
    7: "button1920x1200",
    // PRO: 6 (15kHzScaleDown) and 8 (PassThrough) not supported
    9: "buttonLoadCustomPreset",
  },
  controlKeysMobileMode: "move",
  controlKeysMobile: {
    move: {
      type: "loadDoc",
      left: "7",
      up: "*",
      right: "6",
      down: "/",
    },
    scale: {
      type: "loadDoc",
      left: "h",
      up: "4",
      right: "z",
      down: "5",
    },
    borders: {
      type: "loadUser",
      left: "B",
      up: "C",
      right: "A",
      down: "D",
    },
  },
  dataQueued: 0,
  isWsActive: false,
  maxSlots: 36,  // A-Z, 0-9
  queuedText: "",
  scanSSIDDone: false,
  serverIP: "",
  structs: null,
  timeOutWs: 0,
  ui: {
    backupButton: null,
    backupInput: null,
    customSlotFilters: null,
    developerSwitch: null,
    loader: null,
    outputClear: null,
    presetButtonList: null,
    progressBackup: null,
    progressRestore: null,
    slotButtonList: null,
    slotContainer: null,
    terminal: null,
    toggleList: null,
    toggleSwichList: null,
    webSocketConnectionWarning: null,
    wifiConnect: null,
    wifiConnectButton: null,
    wifiList: null,
    wifiListTable: null,
    wifiPasswordInput: null,
    wifiSSDInput: null,
    wifiApButton: null,
    wifiStaButton: null,
    wifiStaSSID: null,
    alert: null,
    alertOk: null,
    alertContent: null,
    prompt: null,
    promptOk: null,
    promptCancel: null,
    promptContent: null,
    promptInput: null,
  },
  updateTerminalTimer: 0,
  webSocketServerUrl: "",
  wifi: {
    mode: "ap",
    ssid: "",
  },
  ws: null,
  wsCheckTimer: 0,
  wsConnectCounter: 0,
  wsNoSuccessConnectingCounter: 0,
  wsTimeout: 0,
};

/** websocket services */
const checkWebSocketServer = () => {
  if (!GBSControl.isWsActive) {
    if (GBSControl.ws) {
      /*
                        0     CONNECTING
                        1     OPEN
                        2     CLOSING
                        3     CLOSED
                        */
      switch (GBSControl.ws.readyState) {
        case 1:
        case 2:
          GBSControl.ws.close();
          break;
        case 3:
          GBSControl.ws = null;
          break;
      }
    }
    if (!GBSControl.ws) {
      createWebSocket();
    }
  }
};

const timeOutWs = () => {
  console.log("timeOutWs");

  if (GBSControl.ws) {
    GBSControl.ws.close();
  }

  GBSControl.isWsActive = false;
  displayWifiWarning(true);
};

const createWebSocket = () => {
  if (GBSControl.ws && checkReadyState()) {
    return;
  }

  GBSControl.wsNoSuccessConnectingCounter = 0;
  GBSControl.ws = new WebSocket(GBSControl.webSocketServerUrl, ["arduino"]);

  GBSControl.ws.onopen = () => {
    console.log("ws onopen");

    displayWifiWarning(false);

    GBSControl.wsConnectCounter++;
    clearTimeout(GBSControl.wsTimeout);
    GBSControl.wsTimeout = setTimeout(timeOutWs, 6000);
    GBSControl.isWsActive = true;
    GBSControl.wsNoSuccessConnectingCounter = 0;
  };

  GBSControl.ws.onclose = () => {
    console.log("ws.onclose");

    clearTimeout(GBSControl.wsTimeout);
    GBSControl.isWsActive = false;
  };

  GBSControl.ws.onmessage = (message: any) => {
    clearTimeout(GBSControl.wsTimeout);
    GBSControl.wsTimeout = setTimeout(timeOutWs, 2700);
    GBSControl.isWsActive = true;

    const [
      messageDataAt0,
      messageDataAt1,
      messageDataAt2,
      messageDataAt3,
      messageDataAt4,
      messageDataAt5,
    ] = message.data;

    if (messageDataAt0 === "$") {
      // Pro status: $[inputType][format][2x][smooth][sharpness][ace][lumaGain][chromaGain][chromaMax][gammaGain][responseSpeed]
      //             [yFilter][cFilter][wyFilter][wyOverride][comb][hdmiLimitedRange][syncStripper]
      //             [combLumaN][combChromaN][combTapsN][combLumaP][combChromaP][combTapsP][hue]
      // Positions: 0=$ 1=input 2=format 3=2x 4=smooth 5=sharpness 6=ace 7=luma 8=chroma 9=chromamax 10=gamma 11=response
      //            12=yFilter 13=cFilter 14=wyFilter 15=wyOverride 16=comb 17=hdmiLimitedRange 18=syncStripper
      //            19=combLumaN 20=combChromaN 21=combTapsN 22=combLumaP 23=combChromaP 24=combTapsP 25=hue
      const inputType: string = messageDataAt1;
      const formatChar: string = messageDataAt2;
      const line2xChar: string = messageDataAt3;
      const smoothChar: string = messageDataAt4;
      const sharpnessChar: string = messageDataAt5;
      const aceChar: string = message.data[6] || "0";
      const lumaGainChar: string = message.data[7] || "D";  // Default 13 (D in hex-ish)
      const chromaGainChar: string = message.data[8] || "8";
      const chromaMaxChar: string = message.data[9] || "8";
      const gammaGainChar: string = message.data[10] || "8";
      const responseSpeedChar: string = message.data[11] || "F";  // Default 15 (F in hex)

      // Video Filter parameters (positions 12-16)
      const yFilterChar: string = message.data[12] || "1";      // AV default 1 (AutoNarrow)
      const cFilterChar: string = message.data[13] || "0";      // AV default 0 (Auto1.5M)
      const wyFilterChar: string = message.data[14] || "9";     // SV default 9 (SVHS-8), raw value
      const wyOverrideChar: string = message.data[15] || "1";   // SV default 1 (Manual)
      const combFilterChar: string = message.data[16] || "1";   // Default 1 (Medium)

      // HDMI Limited Range (position 17)
      const hdmiLimitedRangeChar: string = message.data[17] || "1";  // Default 1 (HD)

      // Sync Stripper (position 18)
      const syncStripperChar: string = message.data[18] || "1";  // Default 1 (ON)

      // Comb Control parameters (positions 19-24)
      const combLumaNChar: string = message.data[19] || "0";    // NTSC Luma mode
      const combChromaNChar: string = message.data[20] || "0";  // NTSC Chroma mode
      const combTapsNChar: string = message.data[21] || "2";    // NTSC Chroma taps
      const combLumaPChar: string = message.data[22] || "0";    // PAL Luma mode
      const combChromaPChar: string = message.data[23] || "0";  // PAL Chroma mode
      const combTapsPChar: string = message.data[24] || "3";    // PAL Chroma taps

      // Hue parameter (position 25) - encoded as 0-31 (0-254 >> 3)
      const hueChar: string = message.data[25] || "G";          // Default G=16 (128 >> 3)

      // Helper to decode hex char (0-9, A-V for 0-31, A-F for 0-15)
      const fromHexChar = (c: string): number => {
        if (c >= '0' && c <= '9') return parseInt(c, 10);
        if (c >= 'A' && c <= 'V') return c.charCodeAt(0) - 'A'.charCodeAt(0) + 10;
        return 0;
      };

      // Update input source buttons
      const allInputButtons = document.querySelectorAll("[gbs-role='input-source']");
      allInputButtons.forEach((btn) => btn.removeAttribute("active"));

      const currentButton = document.querySelector(`[gbs-pro-i="${inputType}"]`);
      if (currentButton) {
        currentButton.setAttribute("active", "");
      }

      // Show/hide Composite/S-Video Options section based on input type
      // Show section only for S-Video (5) and Composite (6)
      const isCVInput = inputType === "5" || inputType === "6";
      const cvSection = document.getElementById("gbs-pro-cv-section");
      if (cvSection) {
        cvSection.style.display = isCVInput ? "block" : "none";
      }

      // Show/hide ACE Settings section based on input type and ACE enabled
      const aceSection = document.getElementById("gbs-pro-ace-section");
      if (aceSection) {
        aceSection.style.display = (isCVInput && aceChar === "1") ? "block" : "none";
      }

      // Show/hide Video Filters section (always visible for CV input)
      const isSV = inputType === "5";
      const filtersSection = document.getElementById("gbs-pro-filters-section");
      if (filtersSection) {
        filtersSection.style.display = isCVInput ? "block" : "none";
      }

      // Show/hide C Filter vs Override based on input type
      const cFilterRow = document.getElementById("gbs-pro-filter-cfilter-row");
      const overrideRow = document.getElementById("gbs-pro-filter-override-row");
      if (cFilterRow && overrideRow) {
        cFilterRow.style.display = isSV ? "none" : "flex";
        overrideRow.style.display = isSV ? "table" : "none";
      }

      // Update Y Filter value (different for AV vs SV)
      const yFilterValueEl = document.getElementById("gbs-pro-filter-yfilter-value");
      const yFilterDecBtn = document.getElementById("gbs-pro-filter-yfilter-dec");
      const yFilterIncBtn = document.getElementById("gbs-pro-filter-yfilter-inc");
      const isOverrideOn = wyOverrideChar === "1";
      if (yFilterValueEl) {
        if (isSV) {
          // S-Video: when Override is OFF, show "Auto" and disable buttons
          if (isOverrideOn) {
            const wyVal = fromHexChar(wyFilterChar);
            const svIndex = Math.max(0, Math.min(17, wyVal - 2));
            yFilterValueEl.textContent = yFilterNamesSV[svIndex] || "SVHS-8";
          } else {
            yFilterValueEl.textContent = "Auto";
          }
        } else {
          // Composite: yFilterChar is value 0-30
          const yVal = fromHexChar(yFilterChar);
          yFilterValueEl.textContent = yFilterNamesAV[yVal] || "AutoNarrow";
        }
      }
      // Disable Y Filter buttons in SV mode when Override is OFF
      if (yFilterDecBtn && yFilterIncBtn) {
        const disabled = isSV && !isOverrideOn;
        (yFilterDecBtn as HTMLButtonElement).disabled = disabled;
        (yFilterIncBtn as HTMLButtonElement).disabled = disabled;
      }

      // Update C Filter value (AV only)
      const cFilterValueEl = document.getElementById("gbs-pro-filter-cfilter-value");
      if (cFilterValueEl) {
        const cVal = fromHexChar(cFilterChar);
        cFilterValueEl.textContent = cFilterNames[cVal] || "Auto1.5M";
      }

      // Update Override toggle state (SV only)
      const overrideToggle = document.getElementById("gbs-pro-filter-override");
      const overrideTr = document.getElementById("gbs-pro-filter-override-tr");
      if (overrideToggle && overrideTr) {
        const isManual = wyOverrideChar === "1";
        overrideToggle.textContent = isManual ? "toggle_on" : "toggle_off";
        if (isManual) overrideTr.setAttribute("active", "");
        else overrideTr.removeAttribute("active");
      }

      // Update Comb Filter value (in Video Filters section - legacy unified)
      const combValueEl = document.getElementById("gbs-pro-filter-comb-value");
      if (combValueEl) {
        const combVal = fromHexChar(combFilterChar);
        combValueEl.textContent = combNames[combVal] || "Medium";
      }

      // Update HDMI Limited Range value
      const hdmiLimitedRangeValueEl = document.getElementById("gbs-hdmi-limited-range-value");
      if (hdmiLimitedRangeValueEl) {
        const hdmiLimitedRangeNames = ["OFF", "HD", "SD", "ALL"];
        const hdmiVal = parseInt(hdmiLimitedRangeChar, 10);
        hdmiLimitedRangeValueEl.textContent = hdmiLimitedRangeNames[hdmiVal] || "OFF";
      }

      // Update format button
      // Format: 0-9 = '0'-'9', 10 = 'A', 11 = 'B'
      let formatValue: number;
      if (formatChar >= '0' && formatChar <= '9') {
        formatValue = parseInt(formatChar, 10);
      } else if (formatChar === 'A') {
        formatValue = 10;
      } else if (formatChar === 'B') {
        formatValue = 11;
      } else {
        formatValue = 0; // Default to Auto
      }

      const formatButton = document.getElementById("gbs-pro-format");
      if (formatButton) {
        const formatNames = ["Auto", "PAL", "NTSC-M", "PAL-60", "NTSC443", "NTSC-J", "PAL-N w/ p", "PAL-M w/o p", "PAL-M", "PAL Cmb -N", "PAL Cmb -N w/ p", "SECAM"];
        formatButton.setAttribute("gbs-pro-format-value", formatValue.toString());
        const textDiv = formatButton.querySelector("div:not(.gbs-icon)");
        if (textDiv) {
          textDiv.textContent = formatNames[formatValue];
        }
      }

      // Determine if signal is NTSC-family based on format setting
      // NTSC-family: Auto(0), NTSC-M(2), PAL-60(3), NTSC443(4), NTSC-J(5), PAL-M w/o p(7), PAL-M(8)
      const isNTSCSignal = (formatValue === 0 || formatValue === 2 || formatValue === 3 ||
                            formatValue === 4 || formatValue === 5 || formatValue === 7 || formatValue === 8);

      // Comb Control mode/taps name helpers
      const combModeNames = ["Adaptive", "?", "?", "?", "Notch", "Fixed-2L", "Fixed-3L", "Fixed-4L"];
      const chromaModeNames = ["Adaptive", "?", "?", "?", "Off", "Fixed-2L", "Fixed-3L", "Fixed-4L"];
      const combTapsNamesNTSC = ["None", "3->1", "5->3", "5->4"];
      const combTapsNamesPAL = ["None", "3->1", "3->2", "5->4"];

      // Update Comb Control Luma Mode value
      const lumaValueEl = document.getElementById("gbs-pro-filter-luma-value");
      if (lumaValueEl) {
        const lumaVal = fromHexChar(isNTSCSignal ? combLumaNChar : combLumaPChar);
        lumaValueEl.textContent = combModeNames[lumaVal] || "Adaptive";
      }

      // Update Comb Control Chroma Mode value
      const chromaValueEl = document.getElementById("gbs-pro-filter-chroma-value");
      if (chromaValueEl) {
        const chromaVal = fromHexChar(isNTSCSignal ? combChromaNChar : combChromaPChar);
        chromaValueEl.textContent = chromaModeNames[chromaVal] || "Adaptive";
      }

      // Update Comb Control Chroma Taps value
      const tapsValueEl = document.getElementById("gbs-pro-filter-taps-value");
      if (tapsValueEl) {
        const tapsVal = fromHexChar(isNTSCSignal ? combTapsNChar : combTapsPChar);
        const tapsNames = isNTSCSignal ? combTapsNamesNTSC : combTapsNamesPAL;
        tapsValueEl.textContent = tapsNames[tapsVal] || "5->3";
      }

      // Update 2X button
      const btn2x = document.getElementById("gbs-pro-2x");
      const is2XEnabled = line2xChar === "1";

      if (btn2x) {
        if (is2XEnabled) {
          btn2x.setAttribute("active", "");
        } else {
          btn2x.removeAttribute("active");
        }
      }

      // Update Smooth button (smooth only works when 2X is enabled)
      const btnSmooth = document.getElementById("gbs-pro-smooth") as HTMLButtonElement | null;
      if (btnSmooth) {
        // Disable smooth button if 2X is not enabled
        btnSmooth.disabled = !is2XEnabled;
        btnSmooth.style.opacity = is2XEnabled ? "1" : "0.5";

        if (smoothChar === "1" && is2XEnabled) {
          btnSmooth.setAttribute("active", "");
        } else {
          btnSmooth.removeAttribute("active");
        }
      }

      // Update ACE button
      const btnACE = document.getElementById("gbs-pro-ace");
      if (btnACE) {
        if (aceChar === "1") {
          btnACE.setAttribute("active", "");
        } else {
          btnACE.removeAttribute("active");
        }
      }

      // Update ACE parameter values in UI
      const lumaValueSpan = document.getElementById("gbs-pro-ace-luma-value");
      const chromaValueSpan = document.getElementById("gbs-pro-ace-chroma-value");
      const chromamaxValueSpan = document.getElementById("gbs-pro-ace-chromamax-value");
      const gammaValueSpan = document.getElementById("gbs-pro-ace-gamma-value");
      const responseValueSpan = document.getElementById("gbs-pro-ace-response-value");

      if (lumaValueSpan) lumaValueSpan.textContent = fromHexChar(lumaGainChar).toString();
      if (chromaValueSpan) chromaValueSpan.textContent = fromHexChar(chromaGainChar).toString();
      if (chromamaxValueSpan) chromamaxValueSpan.textContent = fromHexChar(chromaMaxChar).toString();
      if (gammaValueSpan) gammaValueSpan.textContent = fromHexChar(gammaGainChar).toString();
      if (responseValueSpan) responseValueSpan.textContent = fromHexChar(responseSpeedChar).toString();

      // Update Sharpness & Peaking Lock
      const isSharpnessActive = sharpnessChar === '1';

      const btnSharpness = document.querySelector('[gbs-toggle="sharpness"]');
      if (btnSharpness) {
        if (isSharpnessActive) {
          btnSharpness.setAttribute("active", "");
        } else {
          btnSharpness.removeAttribute("active");
        }
      }

      // Peaking is locked (disabled) when Sharpness is active
      const btnPeaking = document.querySelector('[gbs-toggle="peaking"]') as HTMLButtonElement | null;
      if (btnPeaking) {
        if (isSharpnessActive) {
          btnPeaking.disabled = true;
          btnPeaking.style.opacity = "0.5";
          btnPeaking.removeAttribute("active"); // Also remove active state if Sharpness locks it
        } else {
          btnPeaking.disabled = false;
          btnPeaking.style.opacity = "1";
        }
      }

      // Update Sync Stripper toggle (only available for RGB inputs, not AV/SV)
      const syncStripperToggle = document.querySelector('[gbs-toggle-switch="syncStripper"]') as HTMLElement | null;
      if (syncStripperToggle) {
        const isSyncStripperOn = syncStripperChar === "1";
        const isSyncStripperAvailable = inputType !== "5" && inputType !== "6"; // Not SV/AV
        syncStripperToggle.textContent = isSyncStripperOn ? "toggle_on" : "toggle_off";
        syncStripperToggle.style.opacity = isSyncStripperAvailable ? "1" : "0.5";
        syncStripperToggle.style.pointerEvents = isSyncStripperAvailable ? "auto" : "none";
        const syncStripperRow = syncStripperToggle.parentElement;
        if (syncStripperRow) {
          if (isSyncStripperOn && isSyncStripperAvailable) {
            syncStripperRow.setAttribute("active", "");
          } else {
            syncStripperRow.removeAttribute("active");
          }
        }
      }

    } else if (messageDataAt0 != "#") {
      GBSControl.queuedText += message.data;
      GBSControl.dataQueued += message.data.length;

      if (GBSControl.dataQueued >= 70000) {
        GBSControl.ui.terminal.value = "";
        GBSControl.dataQueued = 0;
      }
    } else {
      const presetId = GBSControl.buttonMapping[messageDataAt1];
      const presetEl = document.querySelector(
        `[gbs-element-ref="${presetId}"]`
      );
      const activePresetButton = presetEl
        ? presetEl.getAttribute("gbs-element-ref")
        : "none";

      GBSControl.ui.presetButtonList.forEach(
        toggleButtonActive(activePresetButton)
      );

      const slotId = "slot-" + messageDataAt2;
      const activeSlotButton = document.querySelector(
        `[gbs-element-ref="${slotId}"]`
      );

      if (activeSlotButton) {
        GBSControl.ui.slotButtonList.forEach(toggleButtonActive(slotId));
      }

      if (messageDataAt3 && messageDataAt4 && messageDataAt5) {
        const optionByte0 = messageDataAt3.charCodeAt(0);
        const optionByte1 = messageDataAt4.charCodeAt(0);
        const optionByte2 = messageDataAt5.charCodeAt(0);
        const optionButtonList = [
          ...nodelistToArray<HTMLButtonElement>(GBSControl.ui.toggleList),
          ...nodelistToArray<HTMLButtonElement>(GBSControl.ui.toggleSwichList),
        ];

        const toggleMethod = (
          button: HTMLTableCellElement | HTMLElement,
          mode: boolean
        ) => {
          if (button.tagName === "TD") {
            button.innerText = mode ? "toggle_on" : "toggle_off";
          }
          button = button.tagName !== "TD" ? button : button.parentElement;
          if (mode) {
            button.setAttribute("active", "");
          } else {
            button.removeAttribute("active");
          }
        };

        optionButtonList.forEach((button) => {
          const toggleData =
            button.getAttribute("gbs-toggle") ||
            button.getAttribute("gbs-toggle-switch");

          switch (toggleData) {
            case "adcAutoGain":
              toggleMethod(button, (optionByte0 & 0x01) == 0x01);
              break;
            case "scanlines":
              toggleMethod(button, (optionByte0 & 0x02) == 0x02);
              break;
            case "vdsLineFilter":
              toggleMethod(button, (optionByte0 & 0x04) == 0x04);
              break;
            case "peaking":
              toggleMethod(button, (optionByte0 & 0x08) == 0x08);
              break;
            case "palForce60":
              toggleMethod(button, (optionByte0 & 0x10) == 0x10);
              break;
            // PRO: wantOutputComponent not supported
            // case "wantOutputComponent":
            //   toggleMethod(button, (optionByte0 & 0x20) == 0x20);
            //   break;
            /** 1 */

            case "matched":
              toggleMethod(button, (optionByte1 & 0x01) == 0x01);
              break;
            case "frameTimeLock":
              toggleMethod(button, (optionByte1 & 0x02) == 0x02);
              break;
            case "motionAdaptive":
              toggleMethod(button, (optionByte1 & 0x04) == 0x04);
              break;
            case "bob":
              toggleMethod(button, (optionByte1 & 0x04) != 0x04);
              break;
            // case "tap6":
            //   toggleMethod(button, (optionByte1 & 0x08) != 0x04);
            //   break;
            case "step":
              toggleMethod(button, (optionByte1 & 0x10) == 0x10);
              break;
            case "fullHeight":
              toggleMethod(button, (optionByte1 & 0x20) == 0x20);
              break;
            /** 2 */
            case "enableCalibrationADC":
              toggleMethod(button, (optionByte2 & 0x01) == 0x01);
              break;
            // PRO: preferScalingRgbhv not supported
            // case "preferScalingRgbhv":
            //   toggleMethod(button, (optionByte2 & 0x02) == 0x02);
            //   break;
            case "disableExternalClockGenerator":
              toggleMethod(button, (optionByte2 & 0x04) == 0x04);
              break;
          }
        });
      }
    }
  };
};

const checkReadyState = () => {
  if (GBSControl.ws.readyState == 2) {
    GBSControl.wsNoSuccessConnectingCounter++;

    if (GBSControl.wsNoSuccessConnectingCounter >= 7) {
      console.log("ws still closing, force close");
      GBSControl.ws = null;
      GBSControl.wsNoSuccessConnectingCounter = 0;
      /* fall through */
      createWebSocket();
      return false;
    } else {
      return true;
    }
  } else if (GBSControl.ws.readyState == 0) {
    GBSControl.wsNoSuccessConnectingCounter++;

    if (GBSControl.wsNoSuccessConnectingCounter >= 14) {
      console.log("ws still connecting, retry");
      GBSControl.ws.close();
      GBSControl.wsNoSuccessConnectingCounter = 0;
    }
    return true;
  } else {
    return true;
  }
};

const createIntervalChecks = () => {
  GBSControl.wsCheckTimer = setInterval(checkWebSocketServer, 500);
  GBSControl.updateTerminalTimer = setInterval(updateTerminal, 50);
};

/* API services */

const loadDoc = (link: string) => {
  return fetch(
    `http://${GBSControl.serverIP}/sc?${link}&nocache=${new Date().getTime()}`
  );
};

const loadUser = (link: string) => {
  if (link == "a" || link == "1") {
    GBSControl.isWsActive = false;
    GBSControl.ui.terminal.value += "\nRestart\n";
    GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight;
  }

  return fetch(
    `http://${GBSControl.serverIP}/uc?${link}&nocache=${new Date().getTime()}`
  );
};

/** SLOT management */

const savePreset = () => {
  const currentSlot = document.querySelector('[gbs-role="slot"][active]');

  if (!currentSlot) {
    return;
  }

  const key = currentSlot.getAttribute("gbs-element-ref");
  const currentIndex = currentSlot.getAttribute("gbs-slot-id");
  gbsPrompt(
    "Assign a slot name",
    GBSControl.structs.slots[currentIndex].name || key
  )
    .then((currentName: string) => {
      if (currentName && currentName.trim() !== "Empty") {
        currentSlot.setAttribute("gbs-name", currentName);
        fetch(
          `/slot/save?index=${currentIndex}&name=${currentName.substring(
            0,
            24
          )}&${+new Date()}`
        ).then(() => {
          loadUser("4").then(() => {
            setTimeout(() => {
              fetchSlotNames().then((success: boolean) => {
                if (success) {
                  updateSlotNames();
                }
              });
            }, 500);
          });
        });
      }
    })
    .catch(() => {});
};

const loadPreset = () => {
  loadUser("3").then(() => {
    if (GBSStorage.read("customSlotFilters") === true) {
      setTimeout(() => {
        fetch(`/gbs/restore-filters?${+new Date()}`);
      }, 250);
    }
  });
};

const removePreset = () => {
  fetch(`/slot/remove?0&nocache=${new Date().getTime()}`).then(() => {
      setTimeout(() => {
          clearTimeout(GBSControl.wsTimeout);
          // GBSControl.wsTimeout = setTimeout(timeOutWs, 6000); //TODO: calc timeout
          fetch(`/slot/remove?1&nocache=${new Date().getTime()}`).then(() => {
              setTimeout(() => {
                  fetchSlotNames().then((success) => {
                      if (success) {
                          updateSlotNames();
                      }
                  });
              }, 500);
          });
      }, 200);
  });
};

const getSlotsHTML = () => {
  // prettier-ignore
  // PRO: 36 slots only (A-Z = 0-25, 0-9 = 26-35)
  return [
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '0','1','2','3','4','5','6','7','8','9'
  ].map((chr,idx)=>{

    return `<button
    class="gbs-button gbs-button__slot"
    gbs-slot-id="${idx}"
    gbs-message="${chr}"
    gbs-message-type="setSlot"
    gbs-click="normal"
    gbs-element-ref="slot-${chr}"
    gbs-meta="1024&#xa;x768"
    gbs-role="slot"
    gbs-name="slot-${idx}"
  ></button>`;

  }).join('');
};

const setSlot = (slot: string) => {
  fetch(`/slot/set?slot=${slot}&${+new Date()}`);
};

const updateSlotNames = () => {
  const structs = GBSControl.structs as { slots: any[] } | null;
  if (!structs) return;

  // Find the last non-Empty slot (scanning from the end)
  let lastUsedSlotIndex = -1;
  for (let i = GBSControl.maxSlots - 1; i >= 0; i--) {
    if (structs.slots[i].name.trim() !== "Empty") {
      lastUsedSlotIndex = i;
      break;
    }
  }

  // Show slots up to lastUsedSlotIndex + 1 extra Empty slot for new saves
  const visibleSlots = lastUsedSlotIndex + 2; // +1 for index, +1 for extra Empty

  for (let i = 0; i < GBSControl.maxSlots; i++) {
    const el = document.querySelector(`[gbs-slot-id="${i}"]`) as HTMLElement;

    el.setAttribute("gbs-name", structs.slots[i].name);
    el.setAttribute(
      "gbs-meta",
      getSlotPresetName(parseInt(structs.slots[i].presetID, 10))
    );

    // Hide slots beyond the visible range
    if (i < visibleSlots) {
      el.style.display = "";
    } else {
      el.style.display = "none";
    }
  }
};

const fetchSlotNames = () => {
  return fetch(`/bin/slots.bin?${+new Date()}`)
    .then((response) => response.arrayBuffer())
    .then((arrayBuffer: ArrayBuffer) => {
      if (
        arrayBuffer.byteLength ===
        StructParser.getSize(Structs, "slots") * GBSControl.maxSlots
      ) {
        GBSControl.structs = {
          slots: StructParser.parseStructArray(arrayBuffer, Structs, "slots"),
        };
        return true;
      }
      return false;
    });
};

const getSlotPresetName = (presetID: number) => {
  switch (presetID) {
    case 0x01:
    case 0x011:
      return "1280x960";
    case 0x02:
    case 0x012:
      return "1280x1024";
    case 0x03:
    case 0x013:
      return "1280x720";
    case 0x05:
    case 0x015:
      return "1920x1080";
    case 0x07:
    case 0x17:
       return "1920x1200";
    case 0x04:
      return "720x480";
    case 0x14:
      return "768x576";
    // PRO: BYPASS not supported
    // case 0x21: // bypass 1
    // case 0x22: // bypass 2
    //   return "BYPASS";
    default:
      return "CUSTOM";
  }
};

const fetchSlotNamesErrorRetry = () => {
  setTimeout(fetchSlotNamesAndInit, 1000);
};

const fetchSlotNamesAndInit = () => {
  fetchSlotNames()
    .then((success) => {
      if (!success) {
        fetchSlotNamesErrorRetry();
        return;
      }
      initUIElements();
      wifiGetStatus().then(() => {
        initUI();
        updateSlotNames();
        createWebSocket();
        createIntervalChecks();
        setTimeout(hideLoading, 1000);
      });
    }, fetchSlotNamesErrorRetry)
    .catch(fetchSlotNamesErrorRetry);
};

/** Promises */
const serial = (funcs: (() => Promise<any>)[]) =>
  funcs.reduce(
    (promise, func) =>
      promise.then((result) =>
        func().then(Array.prototype.concat.bind(result))
      ),
    Promise.resolve([])
  );

/** helpers */

const toggleHelp = () => {
  let help = GBSStorage.read("help") || false;

  GBSStorage.write("help", !help);
  updateHelp(!help);
};

const toggleDeveloperMode = () => {
  const developerMode = GBSStorage.read("developerMode") || false;

  GBSStorage.write("developerMode", !developerMode);
  updateDeveloperMode(!developerMode);
};

const toggleCustomSlotFilters = () => {
  const customSlotFilters = GBSStorage.read("customSlotFilters");
  GBSStorage.write("customSlotFilters", !customSlotFilters);
  updateCustomSlotFilters(!customSlotFilters);
};

const updateHelp = (help: boolean) => {
  if (help) {
    document.body.classList.remove("gbs-help-hide");
  } else {
    document.body.classList.add("gbs-help-hide");
  }
};

const updateDeveloperMode = (developerMode: boolean) => {
  const el = document.querySelector('[gbs-section="developer"]') as HTMLElement;
  if (developerMode) {
    el.removeAttribute("hidden");
    GBSControl.ui.developerSwitch.setAttribute("active", "");
    document.body.classList.remove("gbs-output-hide");
  } else {
    el.setAttribute("hidden", "");
    GBSControl.ui.developerSwitch.removeAttribute("active");
    document.body.classList.add("gbs-output-hide");
  }

  GBSControl.ui.developerSwitch.querySelector(
    ".gbs-icon"
  ).innerText = developerMode ? "toggle_on" : "toggle_off";
};

const updateCustomSlotFilters = (
  customFilters: boolean = GBSStorage.read("customSlotFilters") === true
) => {
  if (customFilters) {
    GBSControl.ui.customSlotFilters.setAttribute("active", "");
  } else {
    GBSControl.ui.customSlotFilters.removeAttribute("active");
  }

  GBSControl.ui.customSlotFilters.querySelector(
    ".gbs-icon"
  ).innerText = customFilters ? "toggle_on" : "toggle_off";
};

const GBSStorage = {
  lsObject: {},
  write(key: string, value: any) {
    GBSStorage.lsObject = GBSStorage.lsObject || {};
    GBSStorage.lsObject[key] = value;
    localStorage.setItem(
      "GBSControlSlotNames",
      JSON.stringify(GBSStorage.lsObject)
    );
  },
  read(key: string): string | number | boolean {
    GBSStorage.lsObject = JSON.parse(
      localStorage.getItem("GBSControlSlotNames") || "{}"
    );
    return GBSStorage.lsObject[key];
  },
};

const nodelistToArray = <Element>(
  nodelist:
    | HTMLCollectionOf<globalThis.Element>
    | NodeListOf<globalThis.Element>
): Element[] => {
  return Array.prototype.slice.call(nodelist);
};

const toggleButtonActive = (id: string) => (
  button: HTMLElement,
  _index: any,
  _array: any
) => {
  button.removeAttribute("active");

  if (button.getAttribute("gbs-element-ref") === id) {
    button.setAttribute("active", "");
  }
};

const displayWifiWarning = (mode: boolean) => {
  GBSControl.ui.webSocketConnectionWarning.style.display = mode
    ? "block"
    : "none";
};

const updateTerminal = () => {
  if (GBSControl.queuedText.length > 0) {
    requestAnimationFrame(() => {
      GBSControl.ui.terminal.value += GBSControl.queuedText;
      GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight;
      GBSControl.queuedText = "";
    });
  }
};

const updateViewPort = () => {
  document.documentElement.style.setProperty(
    "--viewport-height",
    window.innerHeight + "px"
  );
};

const hideLoading = () => {
  GBSControl.ui.loader.setAttribute("style", "display:none");
};

const checkFetchResponseStatus = (response: Response) => {
  if (!response.ok) {
    throw new Error(`HTTP ${response.status} - ${response.statusText}`);
  }
  return response;
};

const readLocalFile = (file: File) => {
  const reader = new FileReader();
  reader.addEventListener("load", (event) => {
    doRestore(reader.result as ArrayBuffer);
  });
  reader.readAsArrayBuffer(file);
};

/** backup / restore */

const doBackup = () => {
  let backupFiles: string[];
  let done = 0;
  let total = 0;
  fetch("/filesystem/dir")
    .then((r) => r.json())
    .then((files: string[]) => {
      backupFiles = files;
      total = files.length;
      const funcs = files.map((path: string) => () => {
        return fetch(`/filesystem/download?file=${path}&${+new Date()}`).then(
          (response) => {
            GBSControl.ui.progressBackup.setAttribute(
              "gbs-progress",
              `${done}/${total}`
            );
            done++;
            return checkFetchResponseStatus(response) && response.arrayBuffer();
          }
        );
      });

      return serial(funcs);
    })
    .then((files: ArrayBuffer[]) => {
      const headerDescriptor = files.reduce((acc, f, index) => {
        acc[backupFiles[index]] = f.byteLength;
        return acc;
      }, {});

      const backupFilesJSON = JSON.stringify(headerDescriptor);
      const backupFilesJSONSize = backupFilesJSON.length;

      const mainHeader = [
        (backupFilesJSONSize >> 24) & 255, // size
        (backupFilesJSONSize >> 16) & 255, // size
        (backupFilesJSONSize >> 8) & 255, // size
        (backupFilesJSONSize >> 0) & 255,
      ];

      const outputArray: number[] = [
        ...mainHeader,
        ...backupFilesJSON.split("").map((c) => c.charCodeAt(0)),
        ...files.reduce((acc, f, index) => {
          acc = acc.concat(Array.from(new Uint8Array(f)));
          return acc;
        }, []),
      ];

      downloadBlob(
        new Blob([new Uint8Array(outputArray)]),
        `gbs-control.backup-${+new Date()}.bin`
      );
      GBSControl.ui.progressBackup.setAttribute("gbs-progress", ``);
    });
};

const doRestore = (file: ArrayBuffer) => {
  const { backupInput } = GBSControl.ui;
  const fileBuffer = new Uint8Array(file);
  const headerCheck = fileBuffer.slice(4, 6);

  if (headerCheck[0] !== 0x7b || headerCheck[1] !== 0x22) {
    backupInput.setAttribute("disabled", "");
    gbsAlert("Invalid Backup File")
      .then(
        () => {
          backupInput.removeAttribute("disabled");
        },
        () => {
          backupInput.removeAttribute("disabled");
        }
      )
      .catch(() => {
        backupInput.removeAttribute("disabled");
      });
    return;
  }
  const b0 = fileBuffer[0],
    b1 = fileBuffer[1],
    b2 = fileBuffer[2],
    b3 = fileBuffer[3];
  const headerSize = (b0 << 24) + (b1 << 16) + (b2 << 8) + b3;
  const headerString = Array.from(fileBuffer.slice(4, headerSize + 4))
    .map((c) => String.fromCharCode(c))
    .join("");

  const headerObject = JSON.parse(headerString);
  const files = Object.keys(headerObject);
  let pos = headerSize + 4;
  let total = files.length;
  let done = 0;
  const funcs = files.map((fileName) => () => {
    const fileContents = fileBuffer.slice(pos, pos + headerObject[fileName]);
    const formData = new FormData();
    formData.append(
      "file",
      new Blob([fileContents], { type: "application/octet-stream" }),
      fileName.substr(1)
    );

    return fetch("/filesystem/upload", {
      method: "POST",
      body: formData,
    }).then((response) => {
      GBSControl.ui.progressRestore.setAttribute(
        "gbs-progress",
        `${done}/${total}`
      );
      done++;
      pos += headerObject[fileName];
      return response;
    });
  });

  serial(funcs).then(() => {
    GBSControl.ui.progressRestore.setAttribute("gbs-progress", ``);
    loadUser("a").then(() => {
      gbsAlert(
        "Restarting GBSControl.\nPlease wait until wifi reconnects then click OK"
      )
        .then(() => {
          window.location.reload();
        })
        .catch(() => {});
    });
  });
};

const downloadBlob = (blob: Blob, name = "file.txt") => {
  // Convert your blob into a Blob URL (a special url that points to an object in the browser's memory)
  const blobUrl = URL.createObjectURL(blob);

  // Create a link element
  const link = document.createElement("a");

  // Set link's href to point to the Blob URL
  link.href = blobUrl;
  link.download = name;

  // Append link to the body
  document.body.appendChild(link);

  // Dispatch click event on the link
  // This is necessary as link.click() does not work on the latest firefox
  link.dispatchEvent(
    new MouseEvent("click", {
      bubbles: true,
      cancelable: true,
      view: window,
    })
  );

  // Remove link from body
  document.body.removeChild(link);
};

/** WIFI management */
const wifiGetStatus = () => {
  return fetch(`/wifi/status?${+new Date()}`)
    .then((r) => r.json())
    .then((wifiStatus: { mode: string; ssid: string }) => {
      GBSControl.wifi = wifiStatus;
      if (GBSControl.wifi.mode === "ap") {
        GBSControl.ui.wifiApButton.setAttribute("active", "");
        GBSControl.ui.wifiApButton.classList.add("gbs-button__secondary");
        GBSControl.ui.wifiStaButton.removeAttribute("active", "");
        GBSControl.ui.wifiStaButton.classList.remove("gbs-button__secondary");
        GBSControl.ui.wifiStaSSID.innerHTML = "STA | Scan Network";
      } else {
        GBSControl.ui.wifiApButton.removeAttribute("active", "");
        GBSControl.ui.wifiApButton.classList.remove("gbs-button__secondary");
        GBSControl.ui.wifiStaButton.setAttribute("active", "");
        GBSControl.ui.wifiStaButton.classList.add("gbs-button__secondary");
        GBSControl.ui.wifiStaSSID.innerHTML = `${GBSControl.wifi.ssid}`;
      }
    });
};

const wifiConnect = () => {
  const ssid = GBSControl.ui.wifiSSDInput.value;
  const password = GBSControl.ui.wifiPasswordInput.value;

  if (!password.length) {
    GBSControl.ui.wifiPasswordInput.classList.add("gbs-wifi__input--error");
    return;
  }

  const formData = new FormData();
  formData.append("n", ssid);
  formData.append("p", password);

  fetch("/wifi/connect", {
    method: "POST",
    body: formData,
  }).then(() => {
    gbsAlert(
      `GBSControl will restart and will connect to ${ssid}. Please wait some seconds then press OK`
    )
      .then(() => {
        window.location.href = "http://gbscontrol.local/";
      })
      .catch(() => {});
  });
};

const wifiScanSSID = () => {
  GBSControl.ui.wifiStaButton.setAttribute("disabled", "");
  GBSControl.ui.wifiListTable.innerHTML = "";

  if (!GBSControl.scanSSIDDone) {
    fetch(`/wifi/list?${+new Date()}`).then(() => {
      GBSControl.scanSSIDDone = true;
      setTimeout(wifiScanSSID, 3000);
    });
    return;
  }

  fetch(`/wifi/list?${+new Date()}`)
    .then((e) => e.text())
    .then((result) => {
      GBSControl.scanSSIDDone = false;
      return result.length
        ? result
            .split("\n")
            .map((line) => line.split(","))
            .map(([strength, encripted, ssid]) => {
              return { strength, encripted, ssid };
            })
        : [];
    })
    .then((ssids) => {
      return ssids.reduce((acc, ssid) => {
        return `${acc}<tr gbs-ssid="${ssid.ssid}">
        <td class="gbs-icon" style="opacity:${
          parseInt(ssid.strength, 10) / 100
        }">wifi</td>
        <td>${ssid.ssid}</td>
        <td class="gbs-icon">${ssid.encripted ? "lock" : "lock_open"}</td>
      </tr>`;
      }, "");
    })
    .then((html) => {
      GBSControl.ui.wifiStaButton.removeAttribute("disabled");

      if (html.length) {
        GBSControl.ui.wifiListTable.innerHTML = html;
        GBSControl.ui.wifiList.removeAttribute("hidden");
        GBSControl.ui.wifiConnect.setAttribute("hidden", "");
      }
    });
};

const wifiSelectSSID = (event: Event) => {
  (GBSControl.ui
    .wifiSSDInput as HTMLInputElement).value = (event.target as HTMLElement).parentElement.getAttribute(
    "gbs-ssid"
  );
  GBSControl.ui.wifiPasswordInput.classList.remove("gbs-wifi__input--error");
  GBSControl.ui.wifiList.setAttribute("hidden", "");
  GBSControl.ui.wifiConnect.removeAttribute("hidden");
};

const wifiSetAPMode = () => {
  if (GBSControl.wifi.mode === "ap") {
    return;
  }

  const formData = new FormData();
  formData.append("n", "dummy");

  fetch("/wifi/connect", {
    method: "POST",
    body: formData,
  }).then(() => {
    gbsAlert(
      "Switching to AP mode. Please connect to gbscontrol SSID and then click OK"
    )
      .then(() => {
        window.location.href = "http://192.168.4.1";
      })
      .catch(() => {});
  });
};

/** button click management */
const controlClick = (control: HTMLButtonElement) => () => {
  const controlKey = control.getAttribute("gbs-control-key");
  const target = GBSControl.controlKeysMobile[GBSControl.controlKeysMobileMode];

  switch (target.type) {
    case "loadDoc":
      loadDoc(target[controlKey]);
      break;
    case "loadUser":
      loadUser(target[controlKey]);
      break;
  }
};

const controlMouseDown = (control: HTMLButtonElement) => () => {
  clearInterval(control["__interval"]);

  const click = controlClick(control);
  click();
  control["__interval"] = setInterval(click, 300);
};

const controlMouseUp = (control: HTMLButtonElement) => () => {
  clearInterval(control["__interval"]);
};

/** inits */
const initMenuButtons = () => {
  const menuButtons = nodelistToArray<HTMLButtonElement>(
    document.querySelector(".gbs-menu").querySelectorAll("button")
  );
  const sections = nodelistToArray<HTMLElement>(
    document.querySelectorAll("section")
  );
  const scroll = document.querySelector(".gbs-scroll");

  menuButtons.forEach((button) =>
    button.addEventListener("click", () => {
      const section = button.getAttribute("gbs-section");

      sections.forEach((section) => section.setAttribute("hidden", ""));
      document
        .querySelector(`section[name="${section}"]`)
        .removeAttribute("hidden");

      menuButtons.forEach((btn) => btn.removeAttribute("active"));
      button.setAttribute("active", "");
      scroll.scrollTo(0, 1);
    })
  );
};

const initGBSButtons = () => {
  const actions = {
    user: loadUser,
    action: loadDoc,
    setSlot,
  };

  const buttons = nodelistToArray<HTMLElement>(
    document.querySelectorAll("[gbs-click]")
  );

  buttons.forEach((button) => {
    const clickMode = button.getAttribute("gbs-click");
    const message = button.getAttribute("gbs-message");
    const messageType = button.getAttribute("gbs-message-type");
    const action = actions[messageType];

    if (clickMode === "normal") {
      button.addEventListener("click", () => {
        action(message);
      });
    }

    if (clickMode === "repeat") {
      const callback = () => {
        action(message);
      };

      button.addEventListener(
        !("ontouchstart" in window) ? "mousedown" : "touchstart",
        () => {
          callback();
          clearInterval(button["__interval"]);
          button["__interval"] = setInterval(callback, 300);
        }
      );
      button.addEventListener(
        !("ontouchstart" in window) ? "mouseup" : "touchend",
        () => {
          clearInterval(button["__interval"]);
        }
      );
    }
  });
};

const initProButtons = () => {
  const proButtons = document.querySelectorAll("[gbs-pro-i]");

  proButtons.forEach((button) => {
    const i = button.getAttribute("gbs-pro-i");

    button.addEventListener("click", () => {
      const allInputButtons = document.querySelectorAll("[gbs-role='input-source']");
      allInputButtons.forEach((btn) => btn.removeAttribute("active"));

      button.setAttribute("active", "");

      const formData = new URLSearchParams();
      formData.append("i", i || "");

      fetch("/pro", {
        method: "POST",
        body: formData,
      })
        .then((response) => response.text())
        .then((data) => {
          if (data !== "true") {
            console.error("Pro API error:", data);
          }
        })
        .catch((error) => {
          console.error("Pro API error:", error);
        });
    });
  });

  // Handle format button cycling
  const formatButton = document.getElementById("gbs-pro-format");
  if (formatButton) {
    const formatNames = ["Auto", "PAL", "NTSC-M", "PAL-60", "NTSC443", "NTSC-J", "PAL-N w/ p", "PAL-M w/o p", "PAL-M", "PAL Cmb -N", "PAL Cmb -N w/ p", "SECAM"];

    formatButton.addEventListener("click", () => {
      let currentValue = parseInt(formatButton.getAttribute("gbs-pro-format-value") || "0", 10);
      // Cycle to next format (0-11)
      currentValue = (currentValue + 1) % 12;

      // Update button display
      formatButton.setAttribute("gbs-pro-format-value", currentValue.toString());
      const textDiv = formatButton.querySelector("div:not(.gbs-icon)");
      if (textDiv) {
        textDiv.textContent = formatNames[currentValue];
      }

      // Send to API
      const formData = new URLSearchParams();
      formData.append("f", currentValue.toString());

      fetch("/pro", {
        method: "POST",
        body: formData,
      })
        .then((response) => response.text())
        .then((data) => {
          if (data !== "true") {
            console.error("Pro API format error:", data);
          }
        })
        .catch((error) => {
          console.error("Pro API format error:", error);
        });
    });
  }

  // Handle 2X toggle
  const btn2x = document.getElementById("gbs-pro-2x");
  const btnSmooth = document.getElementById("gbs-pro-smooth") as HTMLButtonElement | null;

  if (btn2x) {
    btn2x.addEventListener("click", () => {
      const isActive = btn2x.hasAttribute("active");
      const newState = isActive ? "0" : "1";

      const formData = new URLSearchParams();
      formData.append("x", newState);

      fetch("/pro", {
        method: "POST",
        body: formData,
      })
        .then((response) => response.text())
        .then((data) => {
          if (data === "true") {
            if (newState === "1") {
              btn2x.setAttribute("active", "");
              // Enable Smooth button when 2X is on
              if (btnSmooth) {
                btnSmooth.disabled = false;
                btnSmooth.style.opacity = "1";
              }
            } else {
              btn2x.removeAttribute("active");
              // Disable Smooth button and deactivate when 2X is off
              if (btnSmooth) {
                btnSmooth.disabled = true;
                btnSmooth.style.opacity = "0.5";
                btnSmooth.removeAttribute("active");
              }
            }
          } else {
            console.error("Pro API 2X error:", data);
          }
        })
        .catch((error) => {
          console.error("Pro API 2X error:", error);
        });
    });
  }

  // Handle Smooth toggle
  if (btnSmooth) {
    btnSmooth.addEventListener("click", () => {
      const isActive = btnSmooth.hasAttribute("active");
      const newState = isActive ? "0" : "1";

      const formData = new URLSearchParams();
      formData.append("s", newState);

      fetch("/pro", {
        method: "POST",
        body: formData,
      })
        .then((response) => response.text())
        .then((data) => {
          if (data === "true") {
            if (newState === "1") {
              btnSmooth.setAttribute("active", "");
            } else {
              btnSmooth.removeAttribute("active");
            }
          } else {
            console.error("Pro API Smooth error:", data);
          }
        })
        .catch((error) => {
          console.error("Pro API Smooth error:", error);
        });
    });
  }

  // Handle ACE toggle
  const btnACE = document.getElementById("gbs-pro-ace");
  const aceSection = document.getElementById("gbs-pro-ace-section");
  if (btnACE) {
    btnACE.addEventListener("click", () => {
      const isActive = btnACE.hasAttribute("active");
      const newState = isActive ? "0" : "1";

      const formData = new URLSearchParams();
      formData.append("a", newState);

      fetch("/pro", {
        method: "POST",
        body: formData,
      })
        .then((response) => response.text())
        .then((data) => {
          if (data === "true") {
            if (newState === "1") {
              btnACE.setAttribute("active", "");
              if (aceSection) aceSection.style.display = "block";
            } else {
              btnACE.removeAttribute("active");
              if (aceSection) aceSection.style.display = "none";
            }
          } else {
            console.error("Pro API ACE error:", data);
          }
        })
        .catch((error) => {
          console.error("Pro API ACE error:", error);
        });
    });
  }

  // ACE Settings parameter handlers
  const aceParams: { [key: string]: { param: string; max: number; default: number } } = {
    luma: { param: "al", max: 31, default: 13 },
    chroma: { param: "ac", max: 15, default: 8 },
    chromamax: { param: "am", max: 15, default: 8 },
    gamma: { param: "ag", max: 15, default: 8 },
    response: { param: "ar", max: 15, default: 15 },
  };

  const sendACEParam = (param: string, value: number) => {
    const formData = new URLSearchParams();
    formData.append(param, value.toString());

    fetch("/pro", {
      method: "POST",
      body: formData,
    })
      .then((response) => response.text())
      .then((data) => {
        if (data !== "true") {
          console.error("Pro API ACE param error:", data);
        }
      })
      .catch((error) => {
        console.error("Pro API ACE param error:", error);
      });
  };

  // Setup handlers for each ACE parameter
  const aceParamNames = ["luma", "chroma", "chromamax", "gamma", "response"];
  aceParamNames.forEach((name) => {
    const config = aceParams[name];
    const incBtn = document.getElementById(`gbs-pro-ace-${name}-inc`);
    const decBtn = document.getElementById(`gbs-pro-ace-${name}-dec`);
    const valueSpan = document.getElementById(`gbs-pro-ace-${name}-value`);

    if (incBtn && decBtn && valueSpan) {
      incBtn.addEventListener("click", () => {
        let val = parseInt(valueSpan.textContent || "0", 10);
        if (val < config.max) {
          val++;
          valueSpan.textContent = val.toString();
          sendACEParam(config.param, val);
        }
      });

      decBtn.addEventListener("click", () => {
        let val = parseInt(valueSpan.textContent || "0", 10);
        if (val > 0) {
          val--;
          valueSpan.textContent = val.toString();
          sendACEParam(config.param, val);
        }
      });
    }
  });

  // ACE Reset to Defaults button
  const aceDefaultBtn = document.getElementById("gbs-pro-ace-default");
  if (aceDefaultBtn) {
    aceDefaultBtn.addEventListener("click", () => {
      const formData = new URLSearchParams();
      formData.append("ad", "1");

      fetch("/pro", {
        method: "POST",
        body: formData,
      })
        .then((response) => response.text())
        .then((data) => {
          if (data === "true") {
            // Update UI to show default values
            aceParamNames.forEach((name) => {
              const config = aceParams[name];
              const valueSpan = document.getElementById(`gbs-pro-ace-${name}-value`);
              if (valueSpan) {
                valueSpan.textContent = config.default.toString();
              }
            });
          } else {
            console.error("Pro API ACE defaults error:", data);
          }
        })
        .catch((error) => {
          console.error("Pro API ACE defaults error:", error);
        });
    });
  }

  // Handle Sync Stripper toggle
  const syncStripperToggle = document.querySelector('[gbs-pro-toggle="syncstripper"]');
  if (syncStripperToggle) {
    syncStripperToggle.addEventListener("click", () => {
      const isActive = syncStripperToggle.textContent === "toggle_on";
      const newState = isActive ? "0" : "1";

      const formData = new URLSearchParams();
      formData.append("ss", newState);

      fetch("/pro", {
        method: "POST",
        body: formData,
      })
        .then((response) => response.text())
        .then((data) => {
          if (data === "true") {
            syncStripperToggle.textContent = isActive ? "toggle_off" : "toggle_on";
            const row = syncStripperToggle.parentElement;
            if (row) {
              if (isActive) {
                row.removeAttribute("active");
              } else {
                row.setAttribute("active", "");
              }
            }
          } else {
            console.error("Pro API Sync Stripper error:", data);
          }
        })
        .catch((error) => {
          console.error("Pro API Sync Stripper error:", error);
        });
    });
  }
};

const initClearButton = () => {
  GBSControl.ui.outputClear.addEventListener("click", () => {
    GBSControl.ui.terminal.value = "";
  });
};

const initCustomI2C = () => {
  const input = document.getElementById("customI2CInput") as HTMLInputElement;
  const sendBtn = document.getElementById("customI2CSend") as HTMLButtonElement;

  if (!input || !sendBtn) return;

  const sendCustomI2C = () => {
    const hexStr = input.value.trim();
    if (!hexStr) return;

    // Validate: count bytes, must be multiple of 3 and max 30 (10 triplets)
    const bytes = hexStr.split(',').map(v => v.trim()).filter(v => v);
    if (bytes.length === 0 || bytes.length % 3 !== 0) {
      if (GBSControl.ui.terminal) {
        GBSControl.ui.terminal.value += `> Custom I2C ERROR: must be triplets (addr,reg,val)\n`;
        GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight;
      }
      return;
    }
    if (bytes.length > 30) {
      if (GBSControl.ui.terminal) {
        GBSControl.ui.terminal.value += `> Custom I2C ERROR: max 10 triplets (30 bytes)\n`;
        GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight;
      }
      return;
    }

    const formData = new URLSearchParams();
    formData.append("c", hexStr);

    fetch("/pro", {
      method: "POST",
      body: formData,
    })
      .then((response) => response.text())
      .then((data) => {
        if (data === "true") {
          GBSControl.ui.terminal.value += `> ADV Controller - Custom I2C: ${hexStr}\n`;
          GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight;
        } else {
          GBSControl.ui.terminal.value += `> ADV Controller - Custom I2C ERROR: ${hexStr}\n`;
          GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight;
        }
      })
      .catch((error) => {
        console.error("ADV Controller - Custom I2C error:", error);
        GBSControl.ui.terminal.value += `> ADV Controller - Custom I2C FAILED: ${error}\n`;
      });
  };

  sendBtn.addEventListener("click", sendCustomI2C);
  input.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      sendCustomI2C();
    }
  });
};

// =====================================================================
// Video Filters Button Handlers
// =====================================================================
const initVideoFilters = () => {
  // Helper to send filter parameter to /pro API
  const sendFilterParam = (param: string, value: string | number) => {
    const formData = new URLSearchParams();
    formData.append(param, value.toString());
    fetch("/pro", { method: "POST", body: formData })
      .then((response) => response.text())
      .then((data) => {
        if (data !== "true") console.error("Filter API error:", data);
      })
      .catch((error) => console.error("Filter API error:", error));
  };

  // Helper to check if S-Video mode
  const isSVMode = (): boolean => {
    const overrideRow = document.getElementById("gbs-pro-filter-override-row");
    return overrideRow ? overrideRow.style.display !== "none" : false;
  };

  // Y Filter +/- buttons (different behavior for AV vs SV)
  const yFilterValueEl = document.getElementById("gbs-pro-filter-yfilter-value");
  const yFilterDecBtn = document.getElementById("gbs-pro-filter-yfilter-dec");
  const yFilterIncBtn = document.getElementById("gbs-pro-filter-yfilter-inc");

  const updateYFilter = (delta: number) => {
    if (!yFilterValueEl) return;
    const isSV = isSVMode();

    if (isSV) {
      // S-Video: array index 0-17, raw value 2-19
      const currentName = yFilterValueEl.textContent || "SVHS-8";
      let idx = yFilterNamesSV.indexOf(currentName);
      if (idx < 0) idx = 7;
      idx = Math.max(0, Math.min(yFilterNamesSV.length - 1, idx + delta));
      yFilterValueEl.textContent = yFilterNamesSV[idx];
      sendFilterParam("fy", idx + 2);
    } else {
      // Composite: 0-30
      const currentName = yFilterValueEl.textContent || "AutoNarrow";
      let idx = yFilterNamesAV.indexOf(currentName);
      if (idx < 0) idx = 1;
      idx = Math.max(0, Math.min(yFilterNamesAV.length - 1, idx + delta));
      yFilterValueEl.textContent = yFilterNamesAV[idx];
      sendFilterParam("fy", idx);
    }
  };

  if (yFilterDecBtn) yFilterDecBtn.addEventListener("click", () => updateYFilter(-1));
  if (yFilterIncBtn) yFilterIncBtn.addEventListener("click", () => updateYFilter(1));

  // C Filter +/- buttons (Composite only)
  const cFilterValueEl = document.getElementById("gbs-pro-filter-cfilter-value");
  const cFilterDecBtn = document.getElementById("gbs-pro-filter-cfilter-dec");
  const cFilterIncBtn = document.getElementById("gbs-pro-filter-cfilter-inc");

  const updateCFilter = (delta: number) => {
    if (!cFilterValueEl) return;
    const currentName = cFilterValueEl.textContent || "Auto1.5M";
    let idx = cFilterNames.indexOf(currentName);
    if (idx < 0) idx = 0;
    idx = Math.max(0, Math.min(cFilterNames.length - 1, idx + delta));
    cFilterValueEl.textContent = cFilterNames[idx];
    sendFilterParam("fc", idx);
  };

  if (cFilterDecBtn) cFilterDecBtn.addEventListener("click", () => updateCFilter(-1));
  if (cFilterIncBtn) cFilterIncBtn.addEventListener("click", () => updateCFilter(1));

  // Override toggle (S-Video only)
  const overrideToggle = document.getElementById("gbs-pro-filter-override");
  if (overrideToggle) {
    overrideToggle.addEventListener("click", () => {
      const isManual = overrideToggle.textContent === "toggle_on";
      const newState = isManual ? "0" : "1";
      overrideToggle.textContent = isManual ? "toggle_off" : "toggle_on";
      const row = overrideToggle.parentElement;
      if (row) {
        if (isManual) row.removeAttribute("active");
        else row.setAttribute("active", "");
      }
      // Update Y Filter buttons and value based on new Override state
      const yFilterDecBtn = document.getElementById("gbs-pro-filter-yfilter-dec") as HTMLButtonElement;
      const yFilterIncBtn = document.getElementById("gbs-pro-filter-yfilter-inc") as HTMLButtonElement;
      const yFilterValueEl = document.getElementById("gbs-pro-filter-yfilter-value");
      if (yFilterDecBtn && yFilterIncBtn) {
        yFilterDecBtn.disabled = isManual;  // isManual means we're turning OFF
        yFilterIncBtn.disabled = isManual;
      }
      if (yFilterValueEl && isManual) {
        yFilterValueEl.textContent = "Auto";
      }
      sendFilterParam("fo", newState);
    });
  }

  // Comb Filter +/- buttons
  const combValueEl = document.getElementById("gbs-pro-filter-comb-value");
  const combDecBtn = document.getElementById("gbs-pro-filter-comb-dec");
  const combIncBtn = document.getElementById("gbs-pro-filter-comb-inc");

  const updateCombFilter = (delta: number) => {
    if (!combValueEl) return;
    const currentName = combValueEl.textContent || "Medium";
    let idx = combNames.indexOf(currentName);
    if (idx < 0) idx = 1;
    idx = Math.max(0, Math.min(combNames.length - 1, idx + delta));
    combValueEl.textContent = combNames[idx];
    sendFilterParam("fb", idx);
  };

  if (combDecBtn) combDecBtn.addEventListener("click", () => updateCombFilter(-1));
  if (combIncBtn) combIncBtn.addEventListener("click", () => updateCombFilter(1));

  // Reset to defaults button
  const filterDefaultBtn = document.getElementById("gbs-pro-filter-default");
  if (filterDefaultBtn) {
    filterDefaultBtn.addEventListener("click", () => {
      sendFilterParam("fd", "1");
    });
  }

  // --- Comb Control (in Video Filters section) ---
  // Valid comb modes: 0 (Adaptive), 4 (Notch), 5 (Fixed-2L), 6 (Fixed-3L), 7 (Fixed-4L)
  const validCombModes = [0, 4, 5, 6, 7];
  const combModeNames = ["Adaptive", "?", "?", "?", "Notch", "Fixed-2L", "Fixed-3L", "Fixed-4L"];
  const chromaModeNames = ["Adaptive", "?", "?", "?", "Off", "Fixed-2L", "Fixed-3L", "Fixed-4L"];
  const combTapsNames = ["None", "3->1", "5->3", "5->4"];

  // Luma Mode +/- buttons
  const lumaDecBtn = document.getElementById("gbs-pro-filter-luma-dec");
  const lumaIncBtn = document.getElementById("gbs-pro-filter-luma-inc");
  const lumaValueEl = document.getElementById("gbs-pro-filter-luma-value");

  const updateLumaMode = (delta: number) => {
    if (!lumaValueEl) return;
    const currentName = lumaValueEl.textContent || "Adaptive";
    let currentIdx = combModeNames.indexOf(currentName);
    if (currentIdx < 0) currentIdx = 0;

    let pos = validCombModes.indexOf(currentIdx);
    if (pos < 0) pos = 0;

    pos = (pos + delta + validCombModes.length) % validCombModes.length;
    const newIdx = validCombModes[pos];

    lumaValueEl.textContent = combModeNames[newIdx];
    // Firmware expects "cl" with value suffix: "5n" for NTSC, "5p" for PAL
    sendFilterParam("cl", `${newIdx}n`);
    sendFilterParam("cl", `${newIdx}p`);
  };

  if (lumaDecBtn) lumaDecBtn.addEventListener("click", () => updateLumaMode(-1));
  if (lumaIncBtn) lumaIncBtn.addEventListener("click", () => updateLumaMode(1));

  // Chroma Mode +/- buttons
  const chromaDecBtn = document.getElementById("gbs-pro-filter-chroma-dec");
  const chromaIncBtn = document.getElementById("gbs-pro-filter-chroma-inc");
  const chromaValueEl = document.getElementById("gbs-pro-filter-chroma-value");

  const updateChromaMode = (delta: number) => {
    if (!chromaValueEl) return;
    const currentName = chromaValueEl.textContent || "Adaptive";
    let currentIdx = chromaModeNames.indexOf(currentName);
    if (currentIdx < 0) currentIdx = 0;

    let pos = validCombModes.indexOf(currentIdx);
    if (pos < 0) pos = 0;

    pos = (pos + delta + validCombModes.length) % validCombModes.length;
    const newIdx = validCombModes[pos];

    chromaValueEl.textContent = chromaModeNames[newIdx];
    // Firmware expects "cc" with value suffix: "5n" for NTSC, "5p" for PAL
    sendFilterParam("cc", `${newIdx}n`);
    sendFilterParam("cc", `${newIdx}p`);
  };

  if (chromaDecBtn) chromaDecBtn.addEventListener("click", () => updateChromaMode(-1));
  if (chromaIncBtn) chromaIncBtn.addEventListener("click", () => updateChromaMode(1));

  // Chroma Taps +/- buttons
  const tapsDecBtn = document.getElementById("gbs-pro-filter-taps-dec");
  const tapsIncBtn = document.getElementById("gbs-pro-filter-taps-inc");
  const tapsValueEl = document.getElementById("gbs-pro-filter-taps-value");

  const updateChromaTaps = (delta: number) => {
    if (!tapsValueEl) return;
    const currentName = tapsValueEl.textContent || "5->3";
    let currentIdx = combTapsNames.indexOf(currentName);
    if (currentIdx < 0) currentIdx = 2;

    let newIdx = (currentIdx + delta + 4) % 4;

    tapsValueEl.textContent = combTapsNames[newIdx];
    // Firmware expects "ct" with value suffix: "2n" for NTSC, "2p" for PAL
    sendFilterParam("ct", `${newIdx}n`);
    sendFilterParam("ct", `${newIdx}p`);
  };

  if (tapsDecBtn) tapsDecBtn.addEventListener("click", () => updateChromaTaps(-1));
  if (tapsIncBtn) tapsIncBtn.addEventListener("click", () => updateChromaTaps(1));
};

const initControlMobileKeys = () => {
  const controls = document.querySelectorAll("[gbs-control-target]");
  const controlsKeys = document.querySelectorAll("[gbs-control-key]");

  controls.forEach((control) => {
    control.addEventListener("click", () => {
      GBSControl.controlKeysMobileMode = control.getAttribute(
        "gbs-control-target"
      );
      controls.forEach((crtl) => {
        crtl.removeAttribute("active");
      });
      control.setAttribute("active", "");
    });
  });

  controlsKeys.forEach((control) => {
    control.addEventListener(
      !("ontouchstart" in window) ? "mousedown" : "touchstart",
      controlMouseDown(control as HTMLButtonElement)
    );
    control.addEventListener(
      !("ontouchstart" in window) ? "mouseup" : "touchend",
      controlMouseUp(control as HTMLButtonElement)
    );
  });
};

const initLegendHelpers = () => {
  nodelistToArray<HTMLElement>(
    document.querySelectorAll(".gbs-fieldset__legend--help")
  ).forEach((e) => {
    e.addEventListener("click", toggleHelp);
  });
};

const initUnloadListener = () => {
  window.addEventListener("unload", () => {
    clearInterval(GBSControl.wsCheckTimer);
    if (GBSControl.ws) {
      if (GBSControl.ws.readyState == 0 || GBSControl.ws.readyState == 1) {
        GBSControl.ws.close();
      }
    }
  });
};

const initSlotButtons = () => {
  GBSControl.ui.slotContainer.innerHTML = getSlotsHTML();
  GBSControl.ui.slotButtonList = nodelistToArray(
    document.querySelectorAll('[gbs-role="slot"]')
  ) as HTMLElement[];
};

const initUIElements = () => {
  GBSControl.ui = {
    terminal: document.getElementById("outputTextArea"),
    webSocketConnectionWarning: document.getElementById("websocketWarning"),
    presetButtonList: nodelistToArray(
      document.querySelectorAll("[gbs-role='preset']")
    ) as HTMLElement[],
    slotButtonList: nodelistToArray(
      document.querySelectorAll('[gbs-role="slot"]')
    ) as HTMLElement[],
    toggleList: document.querySelectorAll("[gbs-toggle]"),
    toggleSwichList: document.querySelectorAll("[gbs-toggle-switch]"),
    wifiList: document.querySelector("[gbs-wifi-list]"),
    wifiListTable: document.querySelector(".gbs-wifi__list"),
    wifiConnect: document.querySelector(".gsb-wifi__connect"),
    wifiConnectButton: document.querySelector("[gbs-wifi-connect-button]"),
    wifiSSDInput: document.querySelector('[gbs-input="ssid"]'),
    wifiPasswordInput: document.querySelector('[gbs-input="password"]'),
    wifiApButton: document.querySelector("[gbs-wifi-ap]"),
    wifiStaButton: document.querySelector("[gbs-wifi-station]"),
    wifiStaSSID: document.querySelector("[gbs-wifi-station-ssid]"),
    loader: document.querySelector(".gbs-loader"),
    progressBackup: document.querySelector("[gbs-progress-backup]"),
    progressRestore: document.querySelector("[gbs-progress-restore]"),
    outputClear: document.querySelector("[gbs-output-clear]"),
    slotContainer: document.querySelector("[gbs-slot-html]"),
    backupButton: document.querySelector(".gbs-backup-button"),
    backupInput: document.querySelector(".gbs-backup-input"),
    developerSwitch: document.querySelector("[gbs-dev-switch]"),
    customSlotFilters: document.querySelector("[gbs-slot-custom-filters]"),
    alert: document.querySelector('section[name="alert"]'),
    alertOk: document.querySelector("[gbs-alert-ok]"),
    alertContent: document.querySelector("[gbs-alert-content]"),
    prompt: document.querySelector('section[name="prompt"]'),
    promptOk: document.querySelector("[gbs-prompt-ok]"),
    promptCancel: document.querySelector("[gbs-prompt-cancel]"),
    promptContent: document.querySelector("[gbs-prompt-content]"),
    promptInput: document.querySelector('[gbs-input="prompt-input"]'),
  };
};

const initGeneralListeners = () => {
  window.addEventListener("resize", () => {
    updateViewPort();
  });

  GBSControl.ui.backupInput.addEventListener("change", (event) => {
    const fileList: FileList = event.target["files"];
    readLocalFile(fileList[0]);
    GBSControl.ui.backupInput.value = "";
  });

  GBSControl.ui.backupButton.addEventListener("click", doBackup);
  GBSControl.ui.wifiListTable.addEventListener("click", wifiSelectSSID);
  GBSControl.ui.wifiConnectButton.addEventListener("click", wifiConnect);
  GBSControl.ui.wifiApButton.addEventListener("click", wifiSetAPMode);
  GBSControl.ui.wifiStaButton.addEventListener("click", wifiScanSSID);
  GBSControl.ui.developerSwitch.addEventListener("click", toggleDeveloperMode);
  GBSControl.ui.customSlotFilters.addEventListener(
    "click",
    toggleCustomSlotFilters
  );

  GBSControl.ui.alertOk.addEventListener("click", () => {
    GBSControl.ui.alert.setAttribute("hidden", "");
    gbsAlertPromise.resolve();
  });

  GBSControl.ui.promptOk.addEventListener("click", () => {
    GBSControl.ui.prompt.setAttribute("hidden", "");
    const value = GBSControl.ui.promptInput.value;
    if (value !== undefined || value.length > 0) {
      gbsPromptPromise.resolve(GBSControl.ui.promptInput.value);
    } else {
      gbsPromptPromise.reject();
    }
  });

  GBSControl.ui.promptCancel.addEventListener("click", () => {
    GBSControl.ui.prompt.setAttribute("hidden", "");
    gbsPromptPromise.reject();
  });

  GBSControl.ui.promptInput.addEventListener("keydown", (event: any) => {
    if (event.keyCode === 13) {
      GBSControl.ui.prompt.setAttribute("hidden", "");
      const value = GBSControl.ui.promptInput.value;
      if (value !== undefined || value.length > 0) {
        gbsPromptPromise.resolve(GBSControl.ui.promptInput.value);
      } else {
        gbsPromptPromise.reject();
      }
    }
    if (event.keyCode === 27) {
      gbsPromptPromise.reject();
    }
  });
};

const initDeveloperMode = () => {
  const devMode = GBSStorage.read("developerMode") as boolean;
  if (devMode === undefined) {
    GBSStorage.write("developerMode", false);
    updateDeveloperMode(false);
  } else {
    updateDeveloperMode(devMode);
  }
};

const initHelp = () => {
  let help = GBSStorage.read("help") as boolean;
  if (help === undefined) {
    help = false;
    GBSStorage.write("help", help);
  }
  updateHelp(help);
};

const gbsAlertPromise = {
  resolve: null,
  reject: null,
};

const alertKeyListener = (event: any) => {
  if (event.keyCode === 13) {
    gbsAlertPromise.resolve();
  }
  if (event.keyCode === 27) {
    gbsAlertPromise.reject();
  }
};

const gbsAlert = (text: string) => {
  GBSControl.ui.alertContent.textContent = text;
  GBSControl.ui.alert.removeAttribute("hidden");
  document.addEventListener("keyup", alertKeyListener);
  return new Promise((resolve, reject) => {
    gbsAlertPromise.resolve = (e) => {
      document.removeEventListener("keyup", alertKeyListener);
      GBSControl.ui.alert.setAttribute("hidden", "");
      return resolve(e);
    };
    gbsAlertPromise.reject = () => {
      document.removeEventListener("keyup", alertKeyListener);
      GBSControl.ui.alert.setAttribute("hidden", "");
      return reject();
    };
  });
};

const gbsPromptPromise = {
  resolve: null,
  reject: null,
};

const gbsPrompt = (text: string, defaultValue = "") => {
  GBSControl.ui.promptContent.textContent = text;
  GBSControl.ui.prompt.removeAttribute("hidden");
  GBSControl.ui.promptInput.value = defaultValue;

  return new Promise<string>((resolve, reject) => {
    gbsPromptPromise.resolve = resolve;
    gbsPromptPromise.reject = reject;
    GBSControl.ui.promptInput.focus();
  });
};

const initUI = () => {
  updateCustomSlotFilters();
  initGeneralListeners();
  updateViewPort();
  initSlotButtons();
  initLegendHelpers();
  initMenuButtons();
  initGBSButtons();
  initProButtons();
  initClearButton();
  initCustomI2C();
  initVideoFilters();
  initControlMobileKeys();
  initUnloadListener();
  initDeveloperMode();
  initHelp();
};

const main = () => {
  const ip = location.hostname;
  GBSControl.serverIP = ip;
  GBSControl.webSocketServerUrl = `ws://${ip}:81/`;
  document
    .querySelector(".gbs-loader img")
    .setAttribute(
      "src",
      document.head
        .querySelector('[rel="apple-touch-icon"]')
        .getAttribute("href")
    );
  fetchSlotNamesAndInit();
};

main();
