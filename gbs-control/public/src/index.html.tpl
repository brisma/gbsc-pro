<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <title>GBS-Control Pro</title>
    <link rel="manifest" href="${manifest}" />
    <style>
      ${styles}
    </style>
    <meta name="apple-mobile-web-app-capable" content="yes" />
    <link rel="icon" type="image/png" href="${favicon}" />
    <link rel="apple-touch-icon" href="${icon1024}" />
    <meta name="apple-mobile-web-app-status-bar-style" content="black" />
    <meta
      name="viewport"
      content="viewport-fit=cover, user-scalable=no, width=device-width, initial-scale=1, maximum-scale=1"
    />
  </head>
  <body tabindex="0" class="gbs-help-hide gbs-output-hide">
    <div class="gbs-container">
      <div class="gbs-menu">
        <svg
          version="1.0"
          xmlns="http://www.w3.org/2000/svg"
          viewBox="0,0,284,115"
          class="gbs-menu__logo"
        >
          <path
            fill-rule="evenodd"
            clip-rule="evenodd"
            fill="#010101"
            d="M283.465 114.986H0V0h283.465v114.986z"
          />
          <path
            fill-rule="evenodd"
            clip-rule="evenodd"
            fill="#00c0fb"
            d="M270.062 66.08V51.242h-17.04v10.079c0 2.604-2.67 5.02-5.075 5.02h-20.529c-4.983 0-5.43-4.23-5.43-8.298v-37.93c0-2.668 1.938-4.863 4.88-4.863h20.995c2.684 0 5.158 1.492 5.158 4.482V29.86h17.04V18.63c0-7.867-4.26-15.923-13.039-15.923H221.19c-7.309 0-15.604 4.235-15.604 12.652v50.387c0 6.508 4.883 13.068 12.42 13.068h38.47c6.606 0 13.587-5.803 13.587-12.734zM190.488 2.562H6.617L6.585 78.91h183.91l-.007-76.348z"
          />
          <text
            transform="translate(98.5 68.95)"
            fill="#010101"
            font-family="'AmsiPro-BoldItalic'"
            font-size="80"
            letter-spacing="-7"
            text-anchor="middle"
          >
            GBS
          </text>
          <text
            transform="translate(142 110)"
            fill="#00c0fb"
            font-family="'AmsiPro-BoldItalic'"
            font-size="42"
            letter-spacing="-2"
            font-weight="bold"
            text-anchor="middle"
          >
            PRO
          </text>
          <g>
            <path
              fill-rule="evenodd"
              clip-rule="evenodd"
              fill="#010101"
              d="M586.93 114.986H303.464V0h283.464v114.986z"
            />
            <path
              fill-rule="evenodd"
              clip-rule="evenodd"
              fill="#FFF"
              d="M573.526 66.08V51.242h-17.04v10.079c0 2.604-2.669 5.02-5.075 5.02h-20.528c-4.984 0-5.43-4.23-5.43-8.298v-37.93c0-2.668 1.937-4.863 4.88-4.863h20.995c2.683 0 5.157 1.492 5.157 4.482V29.86h17.04V18.63c0-7.867-4.26-15.923-13.038-15.923h-35.833c-7.31 0-15.605 4.235-15.605 12.652v50.387c0 6.508 4.884 13.068 12.42 13.068h38.471c6.606 0 13.586-5.803 13.586-12.734zM493.953 2.562H310.08l-.032 76.348h183.91l-.006-76.348z"
            />
            <text
              transform="translate(402 68.95)"
              fill="#010101"
              font-family="'AmsiPro-BoldItalic'"
              font-size="80"
              letter-spacing="-7"
              text-anchor="middle"
            >
              GBS
            </text>
            <text
              transform="translate(445.5 110)"
              fill="#FFF"
              font-family="'AmsiPro-BoldItalic'"
              font-size="42"
              letter-spacing="-2"
              font-weight="bold"
              text-anchor="middle"
            >
              PRO
            </text>
          </g>
        </svg>
        <button
          gbs-section="presets"
          class="gbs-button gbs-button__menu gbs-icon"
          active
        >
          input
        </button>
        <button
          gbs-section="control"
          class="gbs-button gbs-button__menu gbs-icon"
        >
          control_camera
        </button>
        <button
          gbs-section="filters"
          class="gbs-button gbs-button__menu gbs-icon"
        >
          blur_on
        </button>
        <button
          gbs-section="preferences"
          class="gbs-button gbs-button__menu gbs-icon"
        >
          tune
        </button>
        <button
          gbs-section="developer"
          class="gbs-button gbs-button__menu gbs-icon"
          hidden
        >
          developer_mode
        </button>
        <button
          gbs-section="system"
          class="gbs-button gbs-button__menu gbs-icon"
        >
          bolt
        </button>
      </div>
      <div class="gbs-scroll">
        <section name="presets">
          <fieldset class="gbs-fieldset" style="padding: 8px 2px">
            <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
              <div class="gbs-icon">aspect_ratio</div>
              <div>Resolution</div>
            </legend>
            <!-- prettier-ignore -->
            <ul class="gbs-help">
              <li>Choose an output resolution from these presets.</li>
              <li>Your selection will also be used for startup. 1280x960 is recommended for NTSC sources, 1280x1024 for PAL.
              </li>
              <li>Use the "Matched Presets" option to switch between the two automatically (Preferences tab)
              </li>
              <li>Selecting a resolution also makes it the new startup preset.</li>
            </ul>
            <div class="gbs-resolution">
            <button
                class="gbs-button gbs-button__resolution"
                gbs-message="L"
                gbs-message-type="user"
                gbs-click="normal"
                gbs-element-ref="button1920x1200"
                gbs-role="preset"
              >
               1920 <span>x1200</span>
              </button>
              <button
                class="gbs-button gbs-button__resolution"
                gbs-message="s"
                gbs-message-type="user"
                gbs-click="normal"
                gbs-element-ref="button1920x1080"
                gbs-role="preset"
              >
                1920 <span>x1080</span>
              </button>
              <button
                class="gbs-button gbs-button__resolution"
                gbs-message="p"
                gbs-message-type="user"
                gbs-click="normal"
                gbs-element-ref="button1280x1024"
                gbs-role="preset"
              >
                1280 <span>x1024</span>
              </button>
              <button
                class="gbs-button gbs-button__resolution"
                gbs-message="f"
                gbs-message-type="user"
                gbs-click="normal"
                gbs-element-ref="button1280x960"
                gbs-role="preset"
              >
                1280 <span>x960</span>
              </button>
              <button
                class="gbs-button gbs-button__resolution"
                gbs-message="g"
                gbs-message-type="user"
                gbs-click="normal"
                gbs-element-ref="button1280x720"
                gbs-role="preset"
              >
                1280 <span>x720</span>
              </button>
              <button
                class="gbs-button gbs-button__resolution"
                gbs-message="h"
                gbs-message-type="user"
                gbs-click="normal"
                gbs-element-ref="button720x480"
                gbs-role="preset"
              >
                480p 576p
              </button>
              <!-- PRO: 15KHz Downscale and Pass Through not supported
              <button
                gbs-message="L"
                gbs-message-type="user"
                gbs-click="normal"
                class="gbs-button gbs-button__resolution gbs-button__resolution--center gbs-button__secondary"
                gbs-element-ref="button15kHzScaleDown"
                gbs-role="preset"
              >
                <div class="gbs-icon">tv</div>
                <div>15KHz</div>
              </button>
              <button
                gbs-message="K"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button gbs-button__resolution gbs-button__resolution--center gbs-button__secondary"
                gbs-element-ref="buttonSourcePassThrough"
                gbs-role="preset"
              >
                <div class="gbs-icon">swap_calls</div>
                <div class="gbs-button__resolution--pass-through">
                  Pass Through
                </div>
              </button>
              -->
            </div>
          </fieldset>
          <fieldset class="gbs-fieldset" style="padding: 8px 2px">
            <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
              <div class="gbs-icon">cable</div>
              <div>Input Source</div>
            </legend>
            <!-- prettier-ignore -->
            <ul class="gbs-help">
              <li>Select the type of video input signal connected to your GBS-Control Pro.</li>
              <li><strong>RGBs</strong>: RGB with separate sync (SCART, etc.)</li>
              <li><strong>RGsB</strong>: RGB with sync on green</li>
              <li><strong>VGA</strong>: VGA/RGBHV computer signal</li>
              <li><strong>YPbPr</strong>: Component video (Y/Pb/Pr)</li>
              <li><strong>S-Video</strong>: S-Video input</li>
              <li><strong>AV</strong>: AV / Composite video</li>
            </ul>
            <div class="gbs-resolution">
              <button
                class="gbs-button gbs-button__resolution"
                gbs-pro-i="1"
                gbs-role="input-source"
              >
                RGBs
              </button>
              <button
                class="gbs-button gbs-button__resolution"
                gbs-pro-i="2"
                gbs-role="input-source"
              >
                RGsB
              </button>
              <button
                class="gbs-button gbs-button__resolution"
                gbs-pro-i="3"
                gbs-role="input-source"
              >
                VGA
              </button>
              <button
                class="gbs-button gbs-button__resolution"
                gbs-pro-i="4"
                gbs-role="input-source"
              >
                YPbPr
              </button>
              <button
                class="gbs-button gbs-button__resolution"
                gbs-pro-i="5"
                gbs-role="input-source"
              >
                S-Video
              </button>
              <button
                class="gbs-button gbs-button__resolution"
                gbs-pro-i="6"
                gbs-role="input-source"
              >
                AV
              </button>
            </div>
          </fieldset>
          <fieldset id="gbs-pro-cv-section" class="gbs-fieldset" style="padding: 8px 2px; display: none;">
            <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
              <div class="gbs-icon">tv</div>
              <div>AV/S-Video Options</div>
            </legend>
            <!-- prettier-ignore -->
            <ul class="gbs-help">
              <li><strong>Format</strong>: Video format/standard for the input signal (Auto recommended)</li>
              <li><strong>2X</strong>: Enable 2X line multiplication for sharper image</li>
              <li><strong>Smooth</strong>: Enable smooth scaling for softer image</li>
              <li><strong>ACE</strong>: Adaptive Contrast Enhancement for improved picture quality</li>
            </ul>
            <div class="gbs-flex gbs-margin__bottom--16">
              <button
                id="gbs-pro-format"
                gbs-pro-format-value="0"
                class="gbs-button gbs-button__control"
                style="flex: 1;"
              >
                <div class="gbs-icon">tv</div>
                <div>Auto</div>
              </button>
              <button
                id="gbs-pro-2x"
                gbs-pro-toggle="2x"
                class="gbs-button gbs-button__control gbs-button__secondary"
                style="flex: 1;"
              >
                <div class="gbs-icon">filter_2</div>
                <div>2X</div>
              </button>
              <button
                id="gbs-pro-smooth"
                gbs-pro-toggle="smooth"
                class="gbs-button gbs-button__control gbs-button__secondary"
                style="flex: 1;"
              >
                <div class="gbs-icon">blur_on</div>
                <div>Smooth</div>
              </button>
              <button
                id="gbs-pro-ace"
                gbs-pro-toggle="ace"
                class="gbs-button gbs-button__control gbs-button__secondary"
                style="flex: 1;"
              >
                <div class="gbs-icon">wb_sunny</div>
                <div>ACE</div>
              </button>
            </div>
          </fieldset>
          <fieldset id="gbs-pro-ace-section" class="gbs-fieldset" style="padding: 8px 2px; display: none;">
            <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
              <div class="gbs-icon">wb_sunny</div>
              <div>ACE Settings</div>
            </legend>
            <!-- prettier-ignore -->
            <ul class="gbs-help">
              <li><strong>Luma Gain</strong>: Automatic contrast enhancement for luminance (0-31, default 13)</li>
              <li><strong>Chroma Gain</strong>: Automatic saturation enhancement (0-15, default 8)</li>
              <li><strong>Chroma Max</strong>: Maximum saturation threshold (0-15, default 8)</li>
              <li><strong>Gamma Gain</strong>: Contrast enhancement via gamma (0-15, default 8)</li>
              <li><strong>Response Speed</strong>: ACE adaptation speed (0-15, default 15)</li>
            </ul>
            <!-- Luma Gain -->
            <div class="gbs-flex gbs-margin__bottom--8">
              <span style="flex: 1; padding: 8px;">Luma Gain</span>
              <button
                id="gbs-pro-ace-luma-dec"
                class="gbs-button gbs-button__control"
                style="width: 40px; height: 42px;"
              >
                <div class="gbs-icon">remove</div>
              </button>
              <span id="gbs-pro-ace-luma-value" style="width: 40px; text-align: center; padding: 8px;">13</span>
              <button
                id="gbs-pro-ace-luma-inc"
                class="gbs-button gbs-button__control"
                style="width: 40px; height: 42px;"
              >
                <div class="gbs-icon">add</div>
              </button>
            </div>
            <!-- Chroma Gain -->
            <div class="gbs-flex gbs-margin__bottom--8">
              <span style="flex: 1; padding: 8px;">Chroma Gain</span>
              <button
                id="gbs-pro-ace-chroma-dec"
                class="gbs-button gbs-button__control"
                style="width: 40px; height: 42px;"
              >
                <div class="gbs-icon">remove</div>
              </button>
              <span id="gbs-pro-ace-chroma-value" style="width: 40px; text-align: center; padding: 8px;">8</span>
              <button
                id="gbs-pro-ace-chroma-inc"
                class="gbs-button gbs-button__control"
                style="width: 40px; height: 42px;"
              >
                <div class="gbs-icon">add</div>
              </button>
            </div>
            <!-- Chroma Max -->
            <div class="gbs-flex gbs-margin__bottom--8">
              <span style="flex: 1; padding: 8px;">Chroma Max</span>
              <button
                id="gbs-pro-ace-chromamax-dec"
                class="gbs-button gbs-button__control"
                style="width: 40px; height: 42px;"
              >
                <div class="gbs-icon">remove</div>
              </button>
              <span id="gbs-pro-ace-chromamax-value" style="width: 40px; text-align: center; padding: 8px;">8</span>
              <button
                id="gbs-pro-ace-chromamax-inc"
                class="gbs-button gbs-button__control"
                style="width: 40px; height: 42px;"
              >
                <div class="gbs-icon">add</div>
              </button>
            </div>
            <!-- Gamma Gain -->
            <div class="gbs-flex gbs-margin__bottom--8">
              <span style="flex: 1; padding: 8px;">Gamma Gain</span>
              <button
                id="gbs-pro-ace-gamma-dec"
                class="gbs-button gbs-button__control"
                style="width: 40px; height: 42px;"
              >
                <div class="gbs-icon">remove</div>
              </button>
              <span id="gbs-pro-ace-gamma-value" style="width: 40px; text-align: center; padding: 8px;">8</span>
              <button
                id="gbs-pro-ace-gamma-inc"
                class="gbs-button gbs-button__control"
                style="width: 40px; height: 42px;"
              >
                <div class="gbs-icon">add</div>
              </button>
            </div>
            <!-- Response Speed -->
            <div class="gbs-flex gbs-margin__bottom--8">
              <span style="flex: 1; padding: 8px;">Response Speed</span>
              <button
                id="gbs-pro-ace-response-dec"
                class="gbs-button gbs-button__control"
                style="width: 40px; height: 42px;"
              >
                <div class="gbs-icon">remove</div>
              </button>
              <span id="gbs-pro-ace-response-value" style="width: 40px; text-align: center; padding: 8px;">15</span>
              <button
                id="gbs-pro-ace-response-inc"
                class="gbs-button gbs-button__control"
                style="width: 40px; height: 42px;"
              >
                <div class="gbs-icon">add</div>
              </button>
            </div>
            <!-- Default button -->
            <div class="gbs-flex">
              <button
                id="gbs-pro-ace-default"
                class="gbs-button gbs-button__control gbs-button__secondary"
                style="flex: 1;"
              >
                <div class="gbs-icon">restore</div>
                <div>Reset to Defaults</div>
              </button>
            </div>
          </fieldset>
          <fieldset id="gbs-pro-filters-section" class="gbs-fieldset" style="padding: 8px 2px; display: none;">
            <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
              <div class="gbs-icon">tune</div>
              <div>Video Filters</div>
            </legend>
            <!-- prettier-ignore -->
            <ul class="gbs-help">
              <li><strong>Y Filter</strong>: Luminance filter shaping</li>
              <li><strong>C Filter</strong>: Chrominance filter (AV only)</li>
              <li><strong>Override</strong>: Manual Y filter control (S-Video only)</li>
              <li><strong>Comb Filter</strong>: Comb filter bandwidth</li>
              <li><strong>Luma/Chr Mode</strong>: Comb filter algorithm</li>
              <li><strong>Chr Taps</strong>: Comb filter line averaging</li>
            </ul>
            <!-- Y Filter -->
            <div class="gbs-flex gbs-margin__bottom--8">
              <span style="flex: 1; padding: 8px;">Y Filter</span>
              <button id="gbs-pro-filter-yfilter-dec" class="gbs-button gbs-button__control" style="width: 40px; height: 42px;">
                <div class="gbs-icon">remove</div>
              </button>
              <span id="gbs-pro-filter-yfilter-value" style="width: 80px; text-align: center; padding: 8px;">AutoNarrow</span>
              <button id="gbs-pro-filter-yfilter-inc" class="gbs-button gbs-button__control" style="width: 40px; height: 42px;">
                <div class="gbs-icon">add</div>
              </button>
            </div>
            <!-- C Filter (AV only) -->
            <div id="gbs-pro-filter-cfilter-row" class="gbs-flex gbs-margin__bottom--8">
              <span style="flex: 1; padding: 8px;">C Filter</span>
              <button id="gbs-pro-filter-cfilter-dec" class="gbs-button gbs-button__control" style="width: 40px; height: 42px;">
                <div class="gbs-icon">remove</div>
              </button>
              <span id="gbs-pro-filter-cfilter-value" style="width: 80px; text-align: center; padding: 8px;">Auto1.5M</span>
              <button id="gbs-pro-filter-cfilter-inc" class="gbs-button gbs-button__control" style="width: 40px; height: 42px;">
                <div class="gbs-icon">add</div>
              </button>
            </div>
            <!-- Override (SV only) -->
            <table id="gbs-pro-filter-override-row" class="gbs-preferences" style="display: none; width: 100%;">
              <tr id="gbs-pro-filter-override-tr">
                <td style="width: 100%; padding: 6px;">Override</td>
                <td id="gbs-pro-filter-override" class="gbs-icon">toggle_off</td>
              </tr>
            </table>
            <!-- Comb Filter -->
            <div class="gbs-flex gbs-margin__bottom--8">
              <span style="flex: 1; padding: 8px;">Comb Filter</span>
              <button id="gbs-pro-filter-comb-dec" class="gbs-button gbs-button__control" style="width: 40px; height: 42px;">
                <div class="gbs-icon">remove</div>
              </button>
              <span id="gbs-pro-filter-comb-value" style="width: 80px; text-align: center; padding: 8px;">Medium</span>
              <button id="gbs-pro-filter-comb-inc" class="gbs-button gbs-button__control" style="width: 40px; height: 42px;">
                <div class="gbs-icon">add</div>
              </button>
            </div>
            <!-- Comb Luma Mode -->
            <div id="gbs-pro-filter-luma-row" class="gbs-flex gbs-margin__bottom--8">
              <span style="flex: 1; padding: 8px;">Luma Mode</span>
              <button id="gbs-pro-filter-luma-dec" class="gbs-button gbs-button__control" style="width: 40px; height: 42px;">
                <div class="gbs-icon">remove</div>
              </button>
              <span id="gbs-pro-filter-luma-value" style="width: 80px; text-align: center; padding: 8px;">Adaptive</span>
              <button id="gbs-pro-filter-luma-inc" class="gbs-button gbs-button__control" style="width: 40px; height: 42px;">
                <div class="gbs-icon">add</div>
              </button>
            </div>
            <!-- Comb Chroma Mode -->
            <div id="gbs-pro-filter-chroma-row" class="gbs-flex gbs-margin__bottom--8">
              <span style="flex: 1; padding: 8px;">Chr Mode</span>
              <button id="gbs-pro-filter-chroma-dec" class="gbs-button gbs-button__control" style="width: 40px; height: 42px;">
                <div class="gbs-icon">remove</div>
              </button>
              <span id="gbs-pro-filter-chroma-value" style="width: 80px; text-align: center; padding: 8px;">Adaptive</span>
              <button id="gbs-pro-filter-chroma-inc" class="gbs-button gbs-button__control" style="width: 40px; height: 42px;">
                <div class="gbs-icon">add</div>
              </button>
            </div>
            <!-- Comb Chroma Taps -->
            <div id="gbs-pro-filter-taps-row" class="gbs-flex gbs-margin__bottom--8">
              <span style="flex: 1; padding: 8px;">Chr Taps</span>
              <button id="gbs-pro-filter-taps-dec" class="gbs-button gbs-button__control" style="width: 40px; height: 42px;">
                <div class="gbs-icon">remove</div>
              </button>
              <span id="gbs-pro-filter-taps-value" style="width: 80px; text-align: center; padding: 8px;">5->3</span>
              <button id="gbs-pro-filter-taps-inc" class="gbs-button gbs-button__control" style="width: 40px; height: 42px;">
                <div class="gbs-icon">add</div>
              </button>
            </div>
            <!-- Reset -->
            <div class="gbs-flex">
              <button id="gbs-pro-filter-default" class="gbs-button gbs-button__control gbs-button__secondary" style="flex: 1;">
                <div class="gbs-icon">restore</div>
                <div>Reset to Defaults</div>
              </button>
            </div>
          </fieldset>
          <fieldset class="gbs-fieldset presets">
            <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
              <div class="gbs-icon">input</div>
              <div>Presets</div>
            </legend>
            <!-- prettier-ignore -->
            <ul class="gbs-help">
              <li>If you want to save your customizations, first select a slot for your new preset, then save to or load from that slot.</li>
              <li>Selecting a slot also makes it the new startup preset.</li>
            </ul>
            <div class="gbs-presets" gbs-slot-html></div>
            <div class="gbs-flex">
              <button
                class="gbs-button gbs-button__control-action"
                active
                gbs-element-ref="buttonLoadCustomPreset"
                gbs-role="preset"
                onclick="loadPreset()"
              >
                <div class="gbs-icon">play_arrow</div>
                <div>load preset</div>
              </button>
              <button
                class="gbs-button gbs-button__control-action gbs-button__secondary"
                onclick="savePreset()"
                active
              >
                <div class="gbs-icon">add_circle_outline</div>
                <div>save preset</div>
              </button>
              <button
                class="gbs-button gbs-button__control-action gbs-button__secondary"
                onclick="removePreset()"
                active
              >
                <div class="gbs-icon">highlight_remove</div>
                <div>remove preset</div>
              </button>
            </div>
          </fieldset>
        </section>

        <section name="control" hidden>
          <fieldset class="gbs-fieldset">
            <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
              <div class="gbs-icon">wb_sunny</div>
              <div>ADC Gain (brightness)</div>
            </legend>
            <!-- prettier-ignore -->
            <ul class="gbs-help">
              <li>Gain +/- adjusts the gain for the currently loaded preset.</li>
              <li>Auto Gain increases gain so bright areas are displayed as white, then decreases it when clipping is detected. Calibrate for a few seconds on a white screen.</li>
            </ul>
            <div class="gbs-flex gbs-margin__bottom--16">
              <button
                gbs-message="o"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control"
              >
                <div class="gbs-icon">remove_circle_outline</div>
                <div>gain</div>
              </button>
              <button
                gbs-message="n"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control"
              >
                <div class="gbs-icon">add_circle_outline</div>
                <div>gain</div>
              </button>
              <button
                gbs-message="T"
                gbs-message-type="action"
                gbs-click="normal"
                gbs-toggle="adcAutoGain"
                class="gbs-button gbs-button__control gbs-button__secondary"
              >
                <div class="gbs-icon">brightness_auto</div>
                <div>Auto Gain</div>
              </button>
            </div>
          </fieldset>
          <fieldset class="gbs-fieldset gbs-controls">
            <legend class="gbs-fieldset__legend">
              <div class="gbs-icon">control_camera</div>
              <div>Picture Control</div>
            </legend>
            <div class="gbs-flex">
              <button
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
                gbs-control-key="left"
              >
                keyboard_arrow_left
              </button>
              <button
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
                gbs-control-key="up"
              >
                keyboard_arrow_up
              </button>
              <button
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
                gbs-control-key="right"
              >
                keyboard_arrow_right
              </button>
            </div>
            <div class="gbs-flex gbs-margin__bottom--16">
              <button class="gbs-button gbs-button__control gbs-icon" disabled>
                south_west
              </button>
              <button
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
                gbs-control-key="down"
              >
                keyboard_arrow_down
              </button>
              <button class="gbs-button gbs-button__control gbs-icon" disabled>
                south_east
              </button>
            </div>
            <div class="gbs-flex">
              <button
                class="gbs-button gbs-button__control"
                gbs-control-target="move"
                active
              >
                <div class="gbs-icon">open_with</div>
                <div>move</div>
              </button>
              <button
                class="gbs-button gbs-button__control"
                gbs-control-target="scale"
              >
                <div class="gbs-icon">zoom_out_map</div>
                <div>scale</div>
              </button>
              <button
                class="gbs-button gbs-button__control"
                gbs-control-target="borders"
              >
                <div class="gbs-icon">crop_free</div>
                <div>borders</div>
              </button>
            </div>
          </fieldset>
          <fieldset class="gbs-fieldset gbs-controls__desktop">
            <legend class="gbs-fieldset__legend">
              <div class="gbs-icon">control_camera</div>
              <div>Picture Control</div>
            </legend>
            <div class="gbs-flex">
              <button
                gbs-message="7"
                gbs-message-type="action"
                gbs-click="repeat"
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
              >
                keyboard_arrow_left
              </button>
              <button
                gbs-message="*"
                gbs-message-type="action"
                gbs-click="repeat"
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
              >
                keyboard_arrow_up
              </button>
              <button
                gbs-message="6"
                gbs-message-type="action"
                gbs-click="repeat"
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
              >
                keyboard_arrow_right
              </button>
            </div>
            <div class="gbs-flex gbs-margin__bottom--16">
              <button class="gbs-button gbs-button__control" active>
                <div class="gbs-icon">open_with</div>
                <div>move</div>
              </button>
              <button
                gbs-message="/"
                gbs-message-type="action"
                gbs-click="repeat"
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
              >
                keyboard_arrow_down
              </button>
              <button class="gbs-button gbs-button__control gbs-icon" disabled>
                south_east
              </button>
            </div>

            <div class="gbs-flex">
              <button
                gbs-message="h"
                gbs-message-type="action"
                gbs-click="repeat"
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
              >
                keyboard_arrow_left
              </button>
              <button
                gbs-message="4"
                gbs-message-type="action"
                gbs-click="repeat"
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
              >
                keyboard_arrow_up
              </button>
              <button
                gbs-message="z"
                gbs-message-type="action"
                gbs-click="repeat"
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
              >
                keyboard_arrow_right
              </button>
            </div>

            <div class="gbs-flex gbs-margin__bottom--16">
              <button class="gbs-button gbs-button__control" active>
                <div class="gbs-icon">zoom_out_map</div>
                <div>scale</div>
              </button>
              <button
                gbs-message="5"
                gbs-message-type="action"
                gbs-click="repeat"
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
              >
                keyboard_arrow_down
              </button>
              <button class="gbs-button gbs-button__control gbs-icon" disabled>
                south_east
              </button>
            </div>
            <div class="gbs-flex">
              <button
                gbs-message="B"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
              >
                keyboard_arrow_left
              </button>
              <button
                gbs-message="C"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
              >
                keyboard_arrow_up
              </button>
              <button
                gbs-message="A"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
              >
                keyboard_arrow_right
              </button>
            </div>

            <div class="gbs-flex gbs-margin__bottom--16">
              <button
                class="gbs-button gbs-button__control"
                gbs-control-target="borders"
                active
              >
                <div class="gbs-icon">crop_free</div>
                <div>borders</div>
              </button>
              <button
                gbs-message="D"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
              >
                keyboard_arrow_down
              </button>
              <button class="gbs-button gbs-button__control gbs-icon" disabled>
                south_east
              </button>
            </div>
          </fieldset>

          <!-- <fieldset class="gbs-fieldset controls-desktop">
            <legend class="gbs-fieldset__legend">
              <div class="gbs-icon">control_camera</div>
              <div>Picture Control</div>
            </legend>
            <div class="">
              <button active class="gbs-button direction">
                <div class="gbs-icon">open_with</div>
                <div>move</div>
              </button>
              <div class="keyboard">
                <div>
                  <button
                    gbs-message="7"
                    gbs-message-type="action"
                    gbs-click="repeat"
                    class="gbs-button gbs-icon gbs-button__secondary"
                  >
                    keyboard_arrow_left
                  </button>
                  <button
                    gbs-message="*"
                    gbs-message-type="action"
                    gbs-click="repeat"
                    class="gbs-button gbs-icon gbs-button__secondary"
                  >
                    keyboard_arrow_up
                  </button>
                  <button
                    gbs-message="6"
                    gbs-message-type="action"
                    gbs-click="repeat"
                    class="gbs-button gbs-icon gbs-button__secondary"
                  >
                    keyboard_arrow_right
                  </button>
                </div>

                <div class="gbs-margin__bottom--16">
                  <button class="gbs-button gbs-icon" disabled>
                    south_west
                  </button>
                  <button
                    gbs-message="/"
                    gbs-message-type="action"
                    gbs-click="repeat"
                    class="gbs-button gbs-icon gbs-button__secondary"
                  >
                    keyboard_arrow_down
                  </button>
                  <button class="gbs-button gbs-icon" disabled>
                    south_east
                  </button>
                </div>
              </div>
            </div>
            <div class="">
              <button class="gbs-button direction" active>
                <div class="gbs-icon">zoom_out_map</div>
                <div>scale</div>
              </button>
              <div class="keyboard">
                <div>
                  <button
                    gbs-message="h"
                    gbs-message-type="action"
                    gbs-click="repeat"
                    class="gbs-button gbs-icon gbs-button__secondary"
                  >
                    keyboard_arrow_left
                  </button>
                  <button
                    gbs-message="4"
                    gbs-message-type="action"
                    gbs-click="repeat"
                    class="gbs-button gbs-icon gbs-button__secondary"
                  >
                    keyboard_arrow_up
                  </button>
                  <button
                    gbs-message="z"
                    gbs-message-type="action"
                    gbs-click="repeat"
                    class="gbs-button gbs-icon gbs-button__secondary"
                  >
                    keyboard_arrow_right
                  </button>
                </div>

                <div class="gbs-margin__bottom--16">
                  <button class="gbs-button gbs-icon" disabled>
                    south_west
                  </button>
                  <button
                    gbs-message="5"
                    gbs-message-type="action"
                    gbs-click="repeat"
                    class="gbs-button gbs-icon gbs-button__secondary"
                  >
                    keyboard_arrow_down
                  </button>
                  <button class="gbs-button gbs-icon" disabled>
                    south_east
                  </button>
                </div>
              </div>
            </div>
            <div class="">
              <button class="gbs-button direction" active>
                <div class="gbs-icon">crop_free</div>
                <div>borders</div>
              </button>
              <div class="keyboard">
                <div>
                  <button
                    gbs-message="B"
                    gbs-message-type="user"
                    gbs-click="repeat"
                    class="gbs-button gbs-icon gbs-button__secondary"
                  >
                    keyboard_arrow_left
                  </button>
                  <button
                    gbs-message="C"
                    gbs-message-type="user"
                    gbs-click="repeat"
                    class="gbs-button gbs-icon gbs-button__secondary"
                  >
                    keyboard_arrow_up
                  </button>
                  <button
                    gbs-message="A"
                    gbs-message-type="user"
                    gbs-click="repeat"
                    class="gbs-button gbs-icon gbs-button__secondary"
                  >
                    keyboard_arrow_right
                  </button>
                </div>

                <div class="gbs-margin__bottom--16">
                  <button class="gbs-button gbs-icon" disabled>
                    south_west
                  </button>
                  <button
                    gbs-message="D"
                    gbs-message-type="user"
                    gbs-click="repeat"
                    class="gbs-button gbs-icon gbs-button__secondary"
                  >
                    keyboard_arrow_down
                  </button>
                  <button class="gbs-button gbs-icon" disabled>
                    south_east
                  </button>
                </div>
              </div>
            </div>
          </fieldset> -->
        </section>

        <section name="filters" hidden>
          <fieldset class="gbs-fieldset filters">
            <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
              <div class="gbs-icon">blur_on</div>
              <div>Filters</div>
            </legend>
            <div class="gbs-margin__bottom--16">
              <div class="gbs-flex gbs-margin__bottom--16">
                <button
                  gbs-message="7"
                  gbs-message-type="user"
                  gbs-click="normal"
                  gbs-toggle="scanlines"
                  class="gbs-button gbs-button__control gbs-button__secondary"
                >
                  <div class="gbs-icon">gradient</div>
                  <div>scanlines</div>
                </button>
                <button
                  gbs-message="K"
                  gbs-message-type="user"
                  gbs-click="normal"
                  class="gbs-button gbs-button__control"
                >
                  <div class="gbs-icon">gradientbolt</div>
                  <div>intensity</div>
                </button>
                <button
                  gbs-message="m"
                  gbs-message-type="user"
                  gbs-click="normal"
                  gbs-toggle="vdsLineFilter"
                  class="gbs-button gbs-button__control gbs-button__secondary"
                >
                  <div class="gbs-icon">power_input</div>
                  <div>line filter</div>
                </button>
              </div>
              <ul class="gbs-help">
                <!-- prettier-ignore -->
                <li>Scanlines only work with 240p sources, or 480i with Bob deinterlacing.</li>
                <li>Line Filter eliminates blocky-pixel artifacts when upscaling beyond 480p, and is recommended.</li>
              </ul>
              <div class="gbs-flex">
                <button
                  gbs-message="f"
                  gbs-message-type="action"
                  gbs-click="normal"
                  gbs-toggle="peaking"
                  class="gbs-button gbs-button__control gbs-button__secondary"
                >
                  <div class="gbs-icon">blur_linear</div>
                  <div>peaking</div>
                </button>
                <button
                  gbs-message="W"
                  gbs-message-type="user"
                  gbs-click="normal"
                  gbs-toggle="sharpness"
                  class="gbs-button gbs-button__control gbs-button__secondary"
                >
                  <div class="gbs-icon">blur_linearbolt</div>
                  <div>sharpness</div>
                </button>
                <button
                  gbs-message="V"
                  gbs-message-type="action"
                  gbs-click="normal"
                  gbs-toggle="step"
                  class="gbs-button gbs-button__control gbs-button__secondary"
                >
                  <div class="gbs-icon">grain</div>
                  <div>step response</div>
                </button>
              </div>
              <ul class="gbs-help">
                <!-- prettier-ignore -->
                <li>Peaking increases contrast around horizontal brightness steps, and is recommended.</li>
                <li>Step Response increases the sharpness of horizontal color steps, and is recommended.</li>
              </ul>
          </fieldset>
          <fieldset class="gbs-fieldset filters">
            <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
              <div class="gbs-icon">wb_sunny</div>
              <div>Сolor correction</div>
            </legend>
            <div class="gbs-flex">
              <button
                gbs-message="Z"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control"
              >
                <div class="gbs-icon">add_circle_outline</div>
                <div>Brightness</div>
              </button>
              <button
                gbs-message="T"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control"
              >
                <div class="gbs-icon">remove_circle_outline</div>
                <div>Brightness</div>
              </button>
            </div>
            <div class="gbs-flex">
              <button
                gbs-message="N"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control"
              >
                <div class="gbs-icon">add_circle_outline</div>
                <div>Contrast</div>
              </button>
              <button
                gbs-message="M"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control"
              >
                <div class="gbs-icon">remove_circle_outline</div>
                <div>Contrast</div>
              </button>
            </div>
            <div class="gbs-flex">
              <button
                gbs-message="Q"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control"
              >
                <div class="gbs-icon">add_circle_outline</div>
                <div>Pb/U gain</div>
              </button>
              <button
                gbs-message="H"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control"
              >
                <div class="gbs-icon">remove_circle_outline</div>
                <div>Pb/U gain</div>
              </button>
            </div>
            <div class="gbs-flex">
              <button
                gbs-message="P"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control"
                >
                <div class="gbs-icon">add_circle_outline</div>
                <div>Pr/V gain</div>
              </button>
              <button
                gbs-message="S"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control"
                >
                <div class="gbs-icon">remove_circle_outline</div>
                <div>Pr/V gain</div>
              </button>
            </div>
            <div class="gbs-flex">
              <button
                gbs-message="O"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control"
              >
              <div class="gbs-icon">info</div>
              <div>Info</div>
              </button>
              <button
                gbs-message="U"
                gbs-message-type="user"
                gbs-click="repeat"
                class="gbs-button gbs-button__control"
              >
                <div class="gbs-icon">settings_backup_restore</div>
                <div>Default</div>
              </button>
            </div>
            <ul class="gbs-help">
              <!-- prettier-ignore -->
              <li>Pb/U gain - change blue-luma gain.</li>
              <li>Pr/V gain - change red-luma gain.</li>
            </ul>
          </fieldset>
        </section>

        <section name="preferences" hidden>
          <fieldset class="gbs-fieldset">
            <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
              <div class="gbs-icon">tune</div>
              <div>Settings</div>
            </legend>
            <table class="gbs-preferences">
              <tr>
                <td>
                  Matched Presets
                  <ul class="gbs-help">
                    <!-- prettier-ignore -->
                    <li>If enabled, default to 1280x960 for NTSC 60 and 1280x1024 for PAL 50 (does not apply for 720p / 1080p presets).</li>
                  </ul>
                </td>
                <td
                  gbs-message="Z"
                  gbs-message-type="action"
                  gbs-click="normal"
                  class="gbs-icon"
                  gbs-toggle-switch="matched"
                >
                  toggle_off
                </td>
              </tr>
              <tr>
                <td>
                  Full Height
                  <!-- prettier-ignore -->
                  <ul class="gbs-help">
                    <li>Some presets default to not using the entire vertical output resolution, leaving some lines black.</li>
                    <li>With Full Height enabled, these presets will instead scale to fill more of the screen height.</li>
                    <li>(This currently only affects 1920 x 1080)</li>
                  </ul>
                </td>
                <td
                  gbs-message="v"
                  gbs-message-type="user"
                  gbs-click="normal"
                  class="gbs-icon"
                  gbs-toggle-switch="fullHeight"
                >
                  toggle_off
                </td>
              </tr>
              <!-- PRO: Low Res Upscaling and Output Component not supported
              <tr>
                <td>
                  Low Res: Use Upscaling
                  <ul class="gbs-help">
                    <li>Low Resolution VGA input: Pass-through or Upscale</li>
                    <li>Low resolution sources can be either passed on directly or get upscaled.</li>
                    <li>Upscaling may have some border / scaling issues, but is more compatible with displays.</li>
                    <li>Also, refresh rates other than 60Hz are not well supported yet.</li>
                    <li>"Low resolution" is currently set at below or equal to 640x480 (525 active lines).</li>
                  </ul>
                </td>
                <td
                  gbs-message="x"
                  gbs-message-type="user"
                  gbs-click="normal"
                  class="gbs-icon"
                  gbs-toggle-switch="preferScalingRgbhv"
                >
                  toggle_off
                </td>
              </tr>
              <tr>
                <td>
                  Output RGBHV/Component
                  <ul class="gbs-help">
                    <li>The default output mode is RGBHV, suitable for use with VGA cables or HDMI converters.</li>
                    <li>An experimental YPbPr mode can also be selected. Compatibility is still spotty.</li>
                  </ul>
                </td>
                <td
                  gbs-message="L"
                  gbs-message-type="action"
                  gbs-click="normal"
                  class="gbs-icon"
                  gbs-toggle-switch="wantOutputComponent"
                >
                  toggle_off
                </td>
              </tr>
              -->
              <tr>
                <td>
                  Output Frame Rate: Force PAL 50Hz to 60Hz
                  <!-- prettier-ignore -->
                  <ul class="gbs-help">
                    <li>If your TV does not support 50Hz sources (displaying unknown format, no matter the preset), try this option.
                    </li>
                    <li>The frame rate will not be as smooth. Reboot required.</li>
                  </ul>
                </td>
                <td
                  gbs-message="0"
                  gbs-message-type="user"
                  gbs-click="normal"
                  class="gbs-icon"
                  gbs-toggle-switch="palForce60"
                >
                  toggle_off
                </td>
              </tr>
              <tr>
                <td>
                  Disable External Clock Generator
                  <!-- prettier-ignore -->
                  <ul class="gbs-help">
                    <li>By default the external clock generator is enabled when installed.</li>
                    <li>You can disable it if you have issues with other options, e.g  Force PAL 50Hz to 60Hz.
                    Reboot required.</li>
                  </ul>
                </td>
                <td
                  gbs-message="X"
                  gbs-message-type="user"
                  gbs-click="normal"
                  class="gbs-icon"
                  gbs-toggle-switch="disableExternalClockGenerator"
                >
                  toggle_off
                </td>
              </tr>
              <tr>
                <td>
                  ADC calibration
                  <!-- prettier-ignore -->
                  <ul class="gbs-help">
                    <li>Gbscontrol calibrates the ADC offsets on startup.</li>
                    <li>In case of color shift problems, try disabling this function.</li>
                  </ul>
                </td>
                <td
                  gbs-message="w"
                  gbs-message-type="user"
                  gbs-click="normal"
                  class="gbs-icon"
                  gbs-toggle-switch="enableCalibrationADC"
                >
                  toggle_off
                </td>
              </tr>
              <tr>
                <td>
                  Sync Stripper
                  <!-- prettier-ignore -->
                  <ul class="gbs-help">
                    <li>Enable LM1881 sync stripper for RGB sources.</li>
                    <li>Useful for sources with dirty or non-standard sync signals.</li>
                  </ul>
                </td>
                <td
                  gbs-pro-toggle="syncstripper"
                  class="gbs-icon"
                  gbs-toggle-switch="syncStripper"
                >
                  toggle_on
                </td>
              </tr>
              <tr>
                <td>
                  HDMI Limited Range
                  <!-- prettier-ignore -->
                  <ul class="gbs-help">
                    <li>Workaround for MS9288 and similar VGA-to-HDMI converters that mark certain resolutions as Limited Range.</li>
                    <li>OFF: No compensation. HD: Apply to 720p/1080p. SD: Apply to 480p/576p/960p/1024p. ALL: Apply to all resolutions.</li>
                    <li>If colors appear washed out or clipped at certain resolutions, try cycling through these options.</li>
                  </ul>
                </td>
                <td
                  gbs-message="%"
                  gbs-message-type="user"
                  gbs-click="normal"
                  style="cursor: pointer; padding-left: 8px;"
                  gbs-hdmi-limited-range
                  id="gbs-hdmi-limited-range-value"
                >
                  OFF
                </td>
              </tr>
              <tr>
                <td colspan="2" class="gbs-preferences__child">
                  Active FrameTime Lock
                  <!-- prettier-ignore -->
                  <ul class="gbs-help">
                    <li>This option keeps the input and output timings aligned, fixing the horizontal tear line that can appear sometimes.</li>
                    <li>Two methods are available. Try switching methods if your display goes blank or shifts vertically.</li>
                  </ul>
                </td>
              </tr>
              <tr>
                <td class="gbs-padding__left-16">FrameTime Lock</td>
                <td
                  class="gbs-icon"
                  gbs-message="5"
                  gbs-message-type="user"
                  gbs-click="normal"
                  gbs-toggle-switch="frameTimeLock"
                >
                  toggle_off
                </td>
              </tr>
              <tr>
                <td class="gbs-padding__left-16">Switch Lock Method</td>
                <td
                  class="gbs-icon"
                  gbs-message="i"
                  gbs-message-type="user"
                  gbs-click="normal"
                  style="cursor: pointer"
                >
                  swap_horiz
                </td>
              </tr>
              <tr>
                <td colspan="2" class="gbs-preferences__child">
                  Deinterlace Method
                  <!-- prettier-ignore -->
                  <ul class="gbs-help">
                    <li>Gbscontrol detects interlaced content and automatically toggles deinterlacing.</li>
                    <li>Bob Method: essentially no deinterlacing, no added lag but flickers, can be combined with scanlines</li>
                    <li>Motion Adaptive: removes flicker and shows some artefacts in moving details</li>
                    <li>If possible, configure the source for progressive output. Otherwise, using Motion Adaptive is recommended.</li>
                  </ul>
                </td>
              </tr>
              <tr>
                <td class="gbs-padding__left-16">Motion Adaptive</td>
                <td
                  gbs-message="r"
                  gbs-message-type="user"
                  gbs-click="normal"
                  class="gbs-icon"
                  gbs-toggle-switch="bob"
                >
                  toggle_off
                </td>
              </tr>
              <tr>
                <td class="gbs-padding__left-16">Bob</td>
                <td
                  gbs-message="q"
                  gbs-message-type="user"
                  gbs-click="normal"
                  class="gbs-icon"
                  gbs-toggle-switch="motionAdaptive"
                >
                  toggle_off
                </td>
              </tr>
              <tr gbs-dev-switch>
                <td>
                  Developer Mode
                  <!-- prettier-ignore -->
                  <ul class="gbs-help">
                    <li>Enables the developer menu which contains various debugging tools</li>
                  </ul>
                </td>
                <td class="gbs-icon">toggle_off</td>
              </tr>
              <tr gbs-slot-custom-filters>
                <td>
                  Save Filtering Per Slot
                  <!-- prettier-ignore -->
                  <ul class="gbs-help">
                    <li>When enabled, saved slots recover their own filter preferences.</li>
                    <li>When disabled, saved slots maintain current filter settings.</li>
                  </ul>
                </td>
                <td class="gbs-icon">toggle_off</td>
              </tr>
              
              <td>
                IR Remote Control: Key Codes
                <!-- prettier-ignore -->
                <ul class="gbs-help">
                  <li>Reading the code of the IR remote control buttons.</li>
                </ul>
              </td>
              <td class="gbs-icon" gbs-message="I" gbs-message-type="user" gbs-click="normal" style="cursor: pointer">
                  swap_horiz
              </td>
            </table>
          </fieldset>
        </section>

        <section name="developer" hidden>
          <fieldset class="gbs-fieldset">
            <legend class="gbs-fieldset__legend">
              <div class="gbs-icon">input</div>
              <div>Developer</div>
            </legend>
            <div class="gbs-flex gbs-margin__bottom--16">
              <button class="gbs-button" gbs-output-toggle>
                <div class="gbs-icon">code</div>
                <div>Toggle Console</div>
              </button>
            </div>
            <div class="gbs-flex gbs-margin__bottom--16">
              <button
                gbs-message="-"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button gbs-button__secondary"
              >
                <div class="gbs-icon">keyboard_arrow_left</div>
                <div>MEM Left</div>
              </button>
              <button
                gbs-message="+"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button gbs-button__secondary"
              >
                <div class="gbs-icon">keyboard_arrow_right</div>
                <div>MEM Right</div>
              </button>
              <button
                gbs-message="1"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button gbs-button__secondary"
              >
                <div class="gbs-icon">keyboard_arrow_left</div>
                <div>HS Left</div>
              </button>
              <button
                gbs-message="0"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button gbs-button__secondary"
              >
                <div class="gbs-icon">keyboard_arrow_right</div>
                <div>HS Right</div>
              </button>
            </div>
            <div class="gbs-flex">
              <button
                gbs-message="e"
                gbs-message-type="user"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">list</div>
                <div>List Options</div>
              </button>
              <button
                gbs-message="i"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">info</div>
                <div>Print Info</div>
              </button>
              <button
                gbs-message=","
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">alarm</div>
                <div>Get Video Timings</div>
              </button>
            </div>

            <div class="gbs-flex">
              <button
                gbs-message="F"
                gbs-message-type="user"
                gbs-click="normal"
                class="gbs-button gbs-margin__bottom--16"
              >
                <div class="gbs-icon">add_a_photo</div>
                <div>Freeze Capture</div>
              </button>
            </div>

            <div class="gbs-flex">
              <button
                gbs-message="F"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">wb_sunny</div>
                <div>ADC Filter</div>
              </button>
              <button
                gbs-message="l"
                gbs-message-type="user"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">memory</div>
                <div>Cycle SDRAM</div>
              </button>
            </div>
            <div class="gbs-flex">
              <button
                gbs-message="D"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">bug_report</div>
                <div>Debug View</div>
              </button>
            </div>
            <div class="gbs-flex">
              <button
                gbs-message="a"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">add_circle_outline</div>
                <div>HTotal++</div>
              </button>
              <button
                gbs-message="A"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">remove_circle_outline</div>
                <div>HTotal--</div>
              </button>
              <button
                gbs-message="."
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button gbs-button__secondary"
              >
                <div class="gbs-icon">sync_problem</div>
                <div>Resync HTotal</div>
              </button>
            </div>
            <div class="gbs-flex">
              <button
                gbs-message="n"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">calculate</div>
                <div>PLL divider++</div>
              </button>
              <button
                gbs-message="8"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">invert_colors</div>
                <div>Invert Sync</div>
              </button>
            </div>
            <div class="gbs-flex">
              <button
                gbs-message="m"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">devices_other</div>
                <div>SyncWatcher</div>
              </button>

              <button
                gbs-message="l"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button gbs-button__secondary"
              >
                <div class="gbs-icon">settings_backup_restore</div>
                <div>SyncProcessor</div>
              </button>
            </div>
            <div class="gbs-flex">
              <button
                gbs-message="o"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">insights</div>
                <div>Oversampling</div>
              </button>
              <button
                gbs-message="S"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">settings_input_hdmi</div>
                <div>60/50Hz HDMI</div>
              </button>

              <button
                gbs-message="E"
                gbs-message-type="user"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">bug_report</div>
                <div>IF Auto Offset</div>
              </button>
            </div>
            <div class="gbs-flex">
              <button
                gbs-message="z"
                gbs-message-type="user"
                gbs-click="normal"
                class="gbs-button"
              >
                <div class="gbs-icon">format_align_justify</div>
                <div>SOG Level--</div>
              </button>

              <button
                gbs-message="q"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button gbs-button__secondary"
              >
                <div class="gbs-icon">model_training</div>
                <div>Reset Chip</div>
              </button>
            </div>
          </fieldset>
        </section>

        <section name="system" hidden>
          <fieldset class="gbs-fieldset">
            <legend class="gbs-fieldset__legend">
              <div class="gbs-icon">bolt</div>
              <div>System</div>
            </legend>
            <div class="gbs-flex">
              <button
                gbs-message="c"
                gbs-message-type="action"
                gbs-click="normal"
                class="gbs-button gbs-button__control"
              >
                <div class="gbs-icon">system_update_alt</div>
                <div>Enable OTA</div>
              </button>
              <button
                gbs-message="a"
                gbs-message-type="user"
                gbs-click="normal"
                class="gbs-button gbs-button__control"
              >
                <div class="gbs-icon">settings_backup_restore</div>
                <div>Restart</div>
              </button>
              <button
                gbs-message="1"
                gbs-message-type="user"
                gbs-click="normal"
                class="gbs-button gbs-button__control gbs-button__secondary"
              >
                <div class="gbs-icon">settings_backup_restore offline_bolt</div>
                <div>Reset Defaults</div>
              </button>
            </div>
          </fieldset>
          <fieldset class="gbs-fieldset">
            <legend class="gbs-fieldset__legend gbs-fieldset__legend--help"">
              <div class="gbs-icon">sd_card</div>
              <div>Backup [intended for same device]</div>
            </legend>
            <!-- prettier-ignore -->
            <ul class="gbs-help">
              <li>Backup / Restore of configuration files</li>
              <li>Backup is valid for current device only</li>
              <!-- <li>Backup is valid between devices with the same hardware revision</li> -->
            </ul>
            <div class="gbs-flex">
              <button
                class="gbs-button gbs-button__control gbs-button__secondary gbs-backup-button"
              >
                <div class="gbs-icon">cloud_download</div>
                <div gbs-progress gbs-progress-backup>Backup</div>
              </button>
              <button
                class="gbs-button gbs-button__control gbs-button__secondary"
              >
                <div class="gbs-icon">cloud_upload</div>
                <input type="file" class="gbs-backup-input" accept=".bin"/>
                <div gbs-progress gbs-progress-restore>Restore</div>
              </button>
            </div>
          </fieldset>
          <fieldset class="gbs-fieldset">
            <legend class="gbs-fieldset__legend">
              <div class="gbs-icon">wifi</div>
              <div>Wi-Fi</div>
            </legend>

            <div class="gbs-flex gbs-margin__bottom--16">
              <button class="gbs-button gbs-button__control" gbs-wifi-ap>
                <div class="gbs-icon">location_on</div>
                <div>Access Point</div>
              </button>
              <button class="gbs-button gbs-button__control" gbs-wifi-station>
                <div class="gbs-icon">radio</div>
                <div gbs-wifi-station-ssid>Station</div>
              </button>
            </div>
            <fieldset class="gbs-fieldset" gbs-wifi-list hidden>
              <legend class="gbs-fieldset__legend">
                <div class="gbs-icon">router</div>
                <div>Select SSID</div>
              </legend>
              <table class="gbs-wifi__list"></table>
            </fieldset>
            <fieldset class="gbs-fieldset gsb-wifi__connect" hidden>
              <legend class="gbs-fieldset__legend">
                <div class="gbs-icon">login</div>
                <div>Connect to SSID</div>
              </legend>
              <div class="gbs-flex">
                <input
                  class="gbs-button gbs-wifi__input"
                  placeholder="SSID"
                  type="text"
                  readonly
                  gbs-input="ssid"
                />
              </div>
              <div class="gbs-flex">
                <input
                  class="gbs-button gbs-wifi__input"
                  placeholder="password"
                  type="password"
                  gbs-input="password"
                />
              </div>
              <div class="gbs-flex">
                <button
                  gbs-wifi-connect-button
                  class="gbs-button gbs-button__control gbs-button__secondary"
                >
                  <div class="gbs-icon">network_check</div>
                  <div>Connect</div>
                </button>
              </div>
            </fieldset>
          </fieldset>
        </section>
        <section name="prompt" hidden>
          <fieldset class="gbs-fieldset">
            <legend class="gbs-fieldset__legend">
              <div class="gbs-icon">keyboard</div>
              <div gbs-prompt-content>Prompt</div>
            </legend>
            <div class="gbs-flex gbs-margin__bottom--16">
              <input
                class="gbs-button"
                type="text"
                gbs-input="prompt-input"
                maxlength="25"
              />
            </div>
            <div class="gbs-flex">
              <button gbs-prompt-cancel class="gbs-button gbs-button__control">
                <div class="gbs-icon">close</div>
                <div>CANCEL</div>
              </button>
              <button
                gbs-prompt-ok
                class="gbs-button gbs-button__control gbs-button__secondary"
              >
                <div class="gbs-icon">done</div>
                <div>OK</div>
              </button>
            </div>
          </fieldset>
        </section>
        <section name="alert" hidden>
          <fieldset class="gbs-fieldset">
            <legend class="gbs-fieldset__legend">
              <div class="gbs-icon">warning</div>
              <div>ALERT</div>
            </legend>
            <div
              class="gbs-flex gbs-padding__hor-16 gbs-modal__message"
              gbs-alert-content
            ></div>
            <div class="gbs-flex">
              <button class="gbs-button gbs-button__control" disabled></button>
              <button
                gbs-alert-ok
                class="gbs-button gbs-button__control gbs-button__secondary"
              >
                <div class="gbs-icon">done</div>
                <div>OK</div>
              </button>
            </div>
          </fieldset>
        </section>
        <div class="gbs-output">
          <fieldset class="gbs-fieldset gbs-fieldset-output">
            <legend class="gbs-fieldset__legend">
              <div class="gbs-icon">code</div>
              <div>Output</div>
            </legend>
            <div class="gbs-flex gbs-margin__bottom--16" gbs-output-clear>
              <button class="gbs-button gbs-icon">delete_outline</button>
            </div>
            <div class="gbs-flex gbs-margin__bottom--16 gbs-custom-i2c">
              <label class="gbs-custom-i2c__label">ADV Controller - Custom I2C</label>
              <input
                type="text"
                id="customI2CInput"
                class="gbs-button gbs-custom-i2c__input"
                placeholder="42,0E,00,42,17,01"
              />
              <button class="gbs-button gbs-custom-i2c__btn" id="customI2CSend" title="Send I2C">
                <span class="gbs-icon">send</span>Send
              </button>
            </div>
            <div class="gbs-flex">
              <textarea
                id="outputTextArea"
                class="gbs-output__textarea"
              ></textarea>
            </div>
          </fieldset>
        </div>
      </div>
      <div class="gbs-loader"><img /></div>
    </div>
    <div class="gbs-wifi-warning" id="websocketWarning">
      <div class="gbs-icon blink_me">signal_wifi_off</div>
    </div>
    <script>
      ${js}
    </script>
  </body>
</html>
