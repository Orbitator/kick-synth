#pragma once
/*
 *  File: synth.h
 *
 *  FM Kickdrum Synthesizer Class.
 *
 *  2023 (c) Your Name
 *
 */

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdlib>

#include <arm_neon.h>

#include "unit.h"  // Note: Include common definitions for all units

// Konstanten definieren
static constexpr float k_pi = 3.14159265358979323846f;
static constexpr float k_twopi = 2.0f * k_pi;
static constexpr float k_samplerate = 48000.0f; // Drumlogue samplerate

// Wellenformen für den zweiten Oszillator
enum {
  k_wave_sine = 0,
  k_wave_saw,
  k_wave_triangle,
  k_wave_pulse,
  k_wave_noise,
  k_num_waves
};

// Hilfsfunktionen für die Wellenformerzeugung
inline float waveformSine(float phase) {
  return std::sin(phase * k_twopi);
}

inline float waveformSaw(float phase) {
  return 2.0f * phase - 1.0f;
}

inline float waveformTriangle(float phase) {
  float ramp = 2.0f * phase - 1.0f;
  return 2.0f * (std::fabs(ramp) - 0.5f);
}

inline float waveformPulse(float phase, float width = 0.5f) {
  return phase < width ? 1.0f : -1.0f;
}

inline float waveformNoise() {
  return ((std::rand() & 0xFFFF) / 32768.0f) * 2.0f - 1.0f;
}

// Wellenform-Namen
static const char * const s_waveform_names[k_num_waves] = {
  "Sine",
  "Saw",
  "Triangle",
  "Pulse",
  "Noise"
};

// Bitmap-Arrays für Wellenformen-Visualisierung
static const uint8_t s_bitmaps[k_num_waves][32] = {
  // Sinus-Wellenform
  {
    0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x03, 0x0C, 0x0C, 0x02, 0x10, 0x01,
    0x20, 0x01, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x20, 0x01,
    0x10, 0x01, 0x0C, 0x02, 0x03, 0x0C, 0x00, 0xF0
  },
  // Sägezahn-Wellenform
  {
    0x00, 0x00, 0x80, 0x00, 0x40, 0x01, 0x20, 0x02, 0x10, 0x04, 0x08, 0x08,
    0x04, 0x10, 0x02, 0x20, 0x01, 0x40, 0x00, 0x80, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  },
  // Dreieck-Wellenform
  {
    0x00, 0x00, 0x00, 0x80, 0x00, 0x40, 0x00, 0x20, 0x00, 0x10, 0x00, 0x08,
    0x00, 0x04, 0x00, 0x02, 0x00, 0x01, 0x80, 0x00, 0x40, 0x00, 0x20, 0x00,
    0x10, 0x00, 0x08, 0x00, 0x04, 0x00, 0x02, 0x00
  },
  // Rechteck-Wellenform
  {
    0x00, 0x00, 0xFF, 0x7F, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40,
    0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40,
    0x01, 0x40, 0x01, 0x40, 0xFF, 0x7F, 0x00, 0x00
  },
  // Noise-Wellenform
  {
    0x00, 0x00, 0x24, 0x82, 0x58, 0x25, 0xA2, 0x50, 0x14, 0x8A, 0x42, 0x51,
    0x85, 0x24, 0x50, 0x8A, 0x24, 0x51, 0x82, 0x24, 0x50, 0x8A, 0x24, 0x51,
    0x44, 0x2A, 0xA8, 0x14, 0x52, 0x42, 0x00, 0x00
  }
};

// Preset-Namen
static const char * const s_preset_names[] = {
  "Basic",
  "Punchy",
  "Sub Bass",
  "FM Kick",
  "Noise Attack"
};

class Synth {
public:
  /*===========================================================================*/
  /* Lifecycle Methods. */
  /*===========================================================================*/

  Synth(void) {
    reset();
    initParams();
  }
  
  ~Synth(void) {}

  inline int8_t Init(const unit_runtime_desc_t * desc) {
    // Check compatibility of samplerate with unit, for drumlogue should be 48000
    if (desc->samplerate != 48000)
      return k_unit_err_samplerate;

    // Check compatibility of frame geometry
    if (desc->output_channels != 2)  // should be stereo output
      return k_unit_err_geometry;

    reset();
    initParams();
    return k_unit_err_none;
  }

  inline void Teardown() {
    // Cleanup is not needed
  }

  inline void Reset() {
    reset();
  }

  inline void Resume() {
    // Nothing to do
  }

  inline void Suspend() {
    // Nothing to do
  }

  /*===========================================================================*/
  /* Core Synth Methods. */
  /*===========================================================================*/

  void reset() {
    phase1_ = 0.f;
    phase2_ = 0.f;
    click_phase_ = 0.f;    // Neue Phase für Click-Oszillator
    envelope_ = 0.f;
    envelope_state_ = k_state_off;
    pitch_envelope_ = 0.f;
    osc2_envelope_ = 0.f;
    current_note_ = 0;
    current_velocity_ = 0.f;
    
    // Filter Reset
    filter_enabled_ = false;
    filter_mode_24db_ = false;
    for (int i = 0; i < 4; ++i) {
      filter_state_[i] = 0.0f;
    }
    
    last_noise_ = 0.f;
    click_envelope_ = 0.f;
  }

  void initParams() {
    // Standardwerte setzen
    pitch_ = 55.f;            // Etwas tiefer für mehr Fülle
    decay_ = 130.f;
    click_level_ = 0.5f;
    pitch_curve_ = 0.5f;
    body_level_ = 0.9f;
    drive_ = 0.4f;
    attack_ = 2.f;
    release_ = 300.f;
    
    // Zweiter Oszillator Parameter
    osc2_enabled_ = 1;
    osc2_waveform_ = k_wave_sine;
    osc2_pitch_ = 2.0f;
    osc2_level_ = 0.5f;
    fm_amount_ = 0.0f;
    fm_ratio_ = 2.0f;
    osc2_decay_ = 100.f;
    pulse_width_ = 0.5f;
    
    // Neue Click Parameter
    click_freq_ = 200.f;      // Frequenz des Click-Oscillators (Hz)
    click_decay_ = 20.f;      // Decay-Zeit des Clicks (ms)
    click_tone_ = 0.6f;       // Toncharakter - 0 = rauschmäßig, 1 = tonal
    
    // Neue Filter Parameter
    filter_enabled_ = false;  // Filter standardmäßig aus
    filter_cutoff_ = 0.7f;    // Anfangsfrequenz 70%
    filter_resonance_ = 0.2f; // Moderate Resonanz
    filter_mode_24db_ = false;// Standard 12dB/Okt
    
    preset_index_ = 0;
    last_noise_ = 0.f;
    click_envelope_ = 0.f;
  }

  fast_inline void Render(float * out, size_t frames) {
    float * __restrict out_p = out;
    const float * out_e = out_p + (frames << 1);  // assuming stereo output

    for (; out_p != out_e; out_p += 2) {
      float value = process();
      out_p[0] = value;  // Left
      out_p[1] = value;  // Right
    }
  }

  float process() {
    float out = 0.f;
    
    // Envelope-Berechnung
    switch (envelope_state_) {
      case k_state_attack:
        envelope_ += (1.f / (attack_ / 1000.f * k_samplerate));
        if (envelope_ >= 1.f) {
          envelope_ = 1.f;
          envelope_state_ = k_state_decay;
        }
        break;
        
      case k_state_decay:
        envelope_ -= (1.f / (release_ / 1000.f * k_samplerate));
        if (envelope_ <= 0.f) {
          envelope_ = 0.f;
          envelope_state_ = k_state_off;
        }
        break;
        
      case k_state_release:
        envelope_ -= (1.f / (release_ / 1000.f * k_samplerate));
        if (envelope_ <= 0.f) {
          envelope_ = 0.f;
          envelope_state_ = k_state_off;
        }
        break;
        
      case k_state_off:
      default:
        break;
    }
    
    // Pitch-Envelope berechnen
    if (envelope_state_ != k_state_off) {
      pitch_envelope_ -= (1.f / (decay_ / 1000.f * k_samplerate));
      if (pitch_envelope_ < 0.f) pitch_envelope_ = 0.f;
      
      // OSC2 Envelope separat berechnen
      osc2_envelope_ -= (1.f / (osc2_decay_ / 1000.f * k_samplerate));
      if (osc2_envelope_ < 0.f) osc2_envelope_ = 0.f;
      
      // Click Envelope separat berechnen
      click_envelope_ -= (1.f / (click_decay_ / 1000.f * k_samplerate));
      if (click_envelope_ < 0.f) click_envelope_ = 0.f;
    }
    
    // Aktuelle Tonhöhe basierend auf Pitch-Envelope berechnen
    float current_pitch = pitch_ * (1.f - pitch_envelope_ * pitch_curve_);
    float freq1 = current_pitch;
    
    // Oszillator 2 Tonhöhe berechnen
    float freq2 = current_pitch * osc2_pitch_;
    
    // *** Frequenzmodulation (FM) berechnen ***
    float fm_mod = 0.f;
    if (osc2_enabled_ && fm_amount_ > 0.f) {
      // Phasenberechnung für den Modulationsoszillator
      float mod_phase = phase2_;
      
      // Wellenform für FM
      float mod_wave = waveformSine(mod_phase); // Sine funktioniert am besten für FM
      
      // FM-Betrag mit Envelope skalieren
      fm_mod = mod_wave * fm_amount_ * osc2_envelope_ * 100.f; // Skalierungsfaktor für deutliche FM
    }
    
    // Haupt-Oszillator-Phase berechnen (mit möglicher FM)
    phase1_ += (freq1 + fm_mod) / k_samplerate;
    if (phase1_ >= 1.f) phase1_ -= 1.f;
    
    // Zweite Oszillator-Phase berechnen
    phase2_ += freq2 * fm_ratio_ / k_samplerate;
    if (phase2_ >= 1.f) phase2_ -= 1.f;
    
    // Click-Oszillator-Phase berechnen
    click_phase_ += click_freq_ / k_samplerate;
    if (click_phase_ >= 1.f) click_phase_ -= 1.f;
    
    // Oszillator 1 (Sinus für den Körper)
    float body = waveformSine(phase1_) * body_level_;
    
    // Oszillator 2 (mit wählbarer Wellenform)
    float osc2_out = 0.f;
    if (osc2_enabled_) {
      switch (osc2_waveform_) {
        case k_wave_sine:
          osc2_out = waveformSine(phase2_);
          break;
        case k_wave_saw:
          osc2_out = waveformSaw(phase2_);
          break;
        case k_wave_triangle:
          osc2_out = waveformTriangle(phase2_);
          break;
        case k_wave_pulse:
          osc2_out = waveformPulse(phase2_, pulse_width_);
          break;
        case k_wave_noise:
          osc2_out = waveformNoise();
          break;
        default:
          osc2_out = waveformSine(phase2_);
          break;
      }
      
      // Envelope auf OSC2 anwenden
      osc2_out *= osc2_level_ * osc2_envelope_;
    }
    
    // --- VERBESSERTE CLICK-GENERATION mit einstellbarer Tonalität ---
    float click = 0.f;
    if (envelope_state_ != k_state_off && click_envelope_ > 0.f) {
      // Noise-Komponente
      float noise = waveformNoise();
      
      // Tonale Komponente (Sinus-Oszillator)
      float tonal = std::sin(click_phase_ * k_twopi);
      
      // Mischung zwischen Noise und Tonal je nach click_tone_
      float click_source = noise * (1.0f - click_tone_) + tonal * click_tone_;
      
      // Hochpassfilter für schärferen Click
      float hp_click = click_source - last_noise_ * 0.7f;
      last_noise_ = click_source;
      
      click = hp_click * click_level_ * click_envelope_ * 3.0f;
    }
    
    // Kombinieren von Body, OSC2 und Click
    out = body + osc2_out + click;
    
    // Soft-Clipping (Drive/Distortion)
    if (drive_ > 0.f) {
      out *= 1.f + drive_ * 4.f;
      out = std::tanh(out) * (1.f / (1.f + drive_ * 1.5f));
    }
    
    // --- Filter anwenden, wenn aktiviert ---
    if (filter_enabled_) {
      float cutoff = filter_cutoff_ * 0.9f + 0.1f; // Min. 10% bis 100%
      float resonance = filter_resonance_ * 0.98f;  // Skaliert bis knapp unter Selbstoszillation
      
      if (filter_mode_24db_) {
        // 24dB/Oct-Filter (4-Pol)
        float f = cutoff * 1.16f;
        float fb = resonance * 4.f * (1.0f - 0.15f * f * f);
        
        float input = out;
        input -= filter_state_[3] * fb;
        input *= 0.35013f * f * f * f * f;
        
        filter_state_[0] = input + 0.3f * filter_state_[0];
        filter_state_[1] = filter_state_[0] + 0.3f * filter_state_[1];
        filter_state_[2] = filter_state_[1] + 0.3f * filter_state_[2];
        filter_state_[3] = filter_state_[2] + 0.3f * filter_state_[3];
        
        out = filter_state_[3];
      } else {
        // 12dB/Oct-Filter (2-Pol)
        float f = cutoff * 1.16f;
        float fb = resonance * 2.5f * (1.0f - 0.2f * f * f);
        
        float input = out;
        input -= filter_state_[1] * fb;
        input *= 0.35013f * f * f;
        
        filter_state_[0] = input + 0.3f * filter_state_[0];
        filter_state_[1] = filter_state_[0] + 0.3f * filter_state_[1];
        
        out = filter_state_[1];
      }
    }
    
    // Verstärktes Signal (grundsätzlich lauter machen)
    out *= 1.3f;
    
    // Envelope anwenden
    out *= envelope_ * current_velocity_;
    
    // Hard-Limiting am Ende
    if (out > 1.0f) out = 1.0f;
    if (out < -1.0f) out = -1.0f;
    
    return out;
  }

  /*===========================================================================*/
  /* Parameter Interface. */
  /*===========================================================================*/

  inline void setParameter(uint8_t index, int32_t value) {
    switch (index) {
      case k_param_pitch:
        pitch_ = value;
        break;
      case k_param_decay:
        decay_ = value;
        break;
      case k_param_body_level:
        body_level_ = value / 100.f;
        break;
      case k_param_drive:
        drive_ = value / 100.f;
        break;
      case k_param_attack:
        attack_ = value;
        break;
      case k_param_release:
        release_ = value;
        break;
      case k_param_pitch_curve:
        pitch_curve_ = value / 100.f;
        break;
      // Click-Parameter (Neu)
      case k_param_click_level:
        click_level_ = value / 100.f;
        break;
      case k_param_click_freq:
        click_freq_ = value;
        break;
      case k_param_click_decay:
        click_decay_ = value;
        break;
      case k_param_click_tone:
        click_tone_ = value / 100.f;
        break;
      // Filter-Parameter (Neu)
      case k_param_filter_enabled:
        filter_enabled_ = value > 0;
        break;
      case k_param_filter_cutoff:
        filter_cutoff_ = value / 100.f;
        break;
      case k_param_filter_resonance:
        filter_resonance_ = value / 100.f;
        break;
      case k_param_filter_mode:
        filter_mode_24db_ = value > 0;
        break;
      // OSC2-Parameter
      case k_param_osc2_enabled:
        osc2_enabled_ = value;
        break;
      case k_param_osc2_waveform:
        osc2_waveform_ = value;
        break;
      case k_param_osc2_pitch:
        osc2_pitch_ = value / 10.f;
        break;
      case k_param_osc2_level:
        osc2_level_ = value / 100.f;
        break;
      case k_param_fm_amount:
        fm_amount_ = value / 100.f;
        break;
      case k_param_fm_ratio:
        fm_ratio_ = value / 10.f;
        break;
      case k_param_osc2_decay:
        osc2_decay_ = value;
        break;
      default:
        break;
    }
  }

  inline int32_t getParameterValue(uint8_t index) const {
    switch (index) {
      case k_param_pitch:
        return pitch_;
      case k_param_decay:
        return decay_;
      case k_param_body_level:
        return body_level_ * 100.f;
      case k_param_drive:
        return drive_ * 100.f;
      case k_param_attack:
        return attack_;
      case k_param_release:
        return release_;
      case k_param_pitch_curve:
        return pitch_curve_ * 100.f;
      // Click-Parameter (Neu)
      case k_param_click_level:
        return click_level_ * 100.f;
      case k_param_click_freq:
        return click_freq_;
      case k_param_click_decay:
        return click_decay_;
      case k_param_click_tone:
        return click_tone_ * 100.f;
      // Filter-Parameter (Neu)
      case k_param_filter_enabled:
        return filter_enabled_ ? 1 : 0;
      case k_param_filter_cutoff:
        return filter_cutoff_ * 100.f;
      case k_param_filter_resonance:
        return filter_resonance_ * 100.f;
      case k_param_filter_mode:
        return filter_mode_24db_ ? 1 : 0;
      // OSC2-Parameter
      case k_param_osc2_enabled:
        return osc2_enabled_;
      case k_param_osc2_waveform:
        return osc2_waveform_;
      case k_param_osc2_pitch:
        return osc2_pitch_ * 10.f;
      case k_param_osc2_level:
        return osc2_level_ * 100.f;
      case k_param_fm_amount:
        return fm_amount_ * 100.f;
      case k_param_fm_ratio:
        return fm_ratio_ * 10.f;
      case k_param_osc2_decay:
        return osc2_decay_;
      default:
        return 0;
    }
  }

  inline const char * getParameterStrValue(uint8_t index, int32_t value) const {
    // String für Wellenform-Parameter zurückgeben
    if (index == k_param_osc2_waveform) {
      uint8_t wave_idx = (uint8_t)value;
      if (wave_idx < k_num_waves) {
        return s_waveform_names[wave_idx];
      }
    }
    return "---";
  }

  inline const uint8_t * getParameterBmpValue(uint8_t index, int32_t value) const {
    // Bitmap für Wellenform-Parameter zurückgeben
    if (index == k_param_osc2_waveform) {
      uint8_t wave_idx = (uint8_t)value;
      if (wave_idx < k_num_waves) {
        return s_bitmaps[wave_idx];
      }
    }
    return s_bitmaps[0]; // Standardmäßig Sinus zurückgeben
  }

  /*===========================================================================*/
  /* MIDI Interface. */
  /*===========================================================================*/

  inline void NoteOn(uint8_t note, uint8_t velocity) {
    current_note_ = note;
    current_velocity_ = velocity / 127.f;
    
    // Envelope auf Attack-Phase setzen
    envelope_state_ = k_state_attack;
    envelope_ = 0.f;
    pitch_envelope_ = 1.f;
    osc2_envelope_ = 1.f;
inline void NoteOn(uint8_t note, uint8_t velocity) {
    current_note_ = note;
    current_velocity_ = velocity / 127.f;
    
    // Envelope auf Attack-Phase setzen
    envelope_state_ = k_state_attack;
    envelope_ = 0.f;
    pitch_envelope_ = 1.f;
    osc2_envelope_ = 1.f;
    click_envelope_ = 1.f;  // Click-Envelope zurücksetzen
    
    // Filter- und Click-Status zurücksetzen
    last_noise_ = 0.f;
    for (int i = 0; i < 4; ++i) {
      filter_state_[i] = 0.0f;
    }
  }

  inline void NoteOff(uint8_t note) {
    if (note == current_note_ || note == 0xFF) {
      envelope_state_ = k_state_release;
    }
  }

  inline void GateOn(uint8_t velocity) {
    NoteOn(0xFF, velocity);
  }

  inline void GateOff() {
    NoteOff(0xFF);
  }

  inline void AllNoteOff() {
    NoteOff(0xFF);
  }

  inline void PitchBend(uint16_t bend) {
    // Nicht verwendet für Kickdrum
    (void)bend;
  }

  inline void ChannelPressure(uint8_t pressure) {
    // Nicht verwendet für Kickdrum
    (void)pressure;
  }

  inline void Aftertouch(uint8_t note, uint8_t aftertouch) {
    // Nicht verwendet für Kickdrum
    (void)note;
    (void)aftertouch;
  }

  /*===========================================================================*/
  /* Preset Interface. */
  /*===========================================================================*/

  inline void LoadPreset(uint8_t index) {
    preset_index_ = index;
    
    switch (index) {
      case k_preset_basic:
        // Grundlegender Kick-Sound mit mehr Punch
        pitch_ = 55.f;
        decay_ = 130.f;
        pitch_curve_ = 0.5f;
        body_level_ = 0.9f;
        drive_ = 0.4f;
        attack_ = 3.f;
        release_ = 300.f;
        
        // Click Parameter
        click_level_ = 0.5f;
        click_freq_ = 200.f;
        click_decay_ = 20.f;
        click_tone_ = 0.6f;
        
        // Filter Parameter
        filter_enabled_ = false;
        filter_cutoff_ = 0.7f;
        filter_resonance_ = 0.2f;
        filter_mode_24db_ = false;
        
        // Abgeschalteter OSC2 für reinen Grund-Kick
        osc2_enabled_ = 0;
        osc2_waveform_ = k_wave_sine;
        osc2_pitch_ = 2.0f;
        osc2_level_ = 0.0f;
        fm_amount_ = 0.0f;
        fm_ratio_ = 2.0f;
        osc2_decay_ = 100.f;
        break;
        
      case k_preset_punchy:
        // Punchiger Kick-Sound mit mehr Knackigkeit
        pitch_ = 70.f;
        decay_ = 80.f;
        pitch_curve_ = 0.7f;
        body_level_ = 0.9f;
        drive_ = 0.6f;
        attack_ = 1.f;
        release_ = 180.f;
        
        // Click Parameter
        click_level_ = 0.8f;
        click_freq_ = 250.f;
        click_decay_ = 15.f;
        click_tone_ = 0.5f;
        
        // Filter Parameter
        filter_enabled_ = true;
        filter_cutoff_ = 0.9f;
        filter_resonance_ = 0.3f;
        filter_mode_24db_ = false;
        
        osc2_enabled_ = 0;
        osc2_waveform_ = k_wave_sine;
        osc2_pitch_ = 2.0f;
        osc2_level_ = 0.0f;
        fm_amount_ = 0.0f;
        fm_ratio_ = 2.0f;
        osc2_decay_ = 100.f;
        break;
        
      case k_preset_sub:
        // Tiefer Sub-Bass Kick mit mehr Wärme
        pitch_ = 45.f;
        decay_ = 250.f;
        pitch_curve_ = 0.3f;
        body_level_ = 0.95f;
        drive_ = 0.35f;
        attack_ = 8.f;
        release_ = 500.f;
        
        // Click Parameter
        click_level_ = 0.3f;
        click_freq_ = 180.f;
        click_decay_ = 25.f;
        click_tone_ = 0.7f;
        
        // Filter Parameter
        filter_enabled_ = true;
        filter_cutoff_ = 0.6f;
        filter_resonance_ = 0.1f;
        filter_mode_24db_ = true;
        
        osc2_enabled_ = 0;
        osc2_waveform_ = k_wave_sine;
        osc2_pitch_ = 2.0f;
        osc2_level_ = 0.0f;
        fm_amount_ = 0.0f;
        fm_ratio_ = 2.0f;
        osc2_decay_ = 100.f;
        break;

      case k_preset_fm_kick:
        // FM-Kick mit zweitem Oszillator und komplexer Klangfarbe
        pitch_ = 55.f;
        decay_ = 180.f;
        pitch_curve_ = 0.6f;
        body_level_ = 0.7f;
        drive_ = 0.5f;
        attack_ = 3.f;
        release_ = 250.f;
        
        // Click Parameter
        click_level_ = 0.4f;
        click_freq_ = 220.f;
        click_decay_ = 18.f;
        click_tone_ = 0.8f;
        
        // Filter Parameter
        filter_enabled_ = true;
        filter_cutoff_ = 0.85f;
        filter_resonance_ = 0.4f;
        filter_mode_24db_ = false;
        
        osc2_enabled_ = 1;
        osc2_waveform_ = k_wave_sine;
        osc2_pitch_ = 3.0f;
        osc2_level_ = 0.6f;
        fm_amount_ = 0.7f;
        fm_ratio_ = 2.7f;
        osc2_decay_ = 80.f;
        break;

      case k_preset_noise_attack:
        // Kick mit Noise-Attack und starkem Punch
        pitch_ = 50.f;
        decay_ = 200.f;
        pitch_curve_ = 0.5f;
        body_level_ = 0.85f;
        drive_ = 0.4f;
        attack_ = 2.f;
        release_ = 280.f;
        
        // Click Parameter
        click_level_ = 0.7f;
        click_freq_ = 300.f;
        click_decay_ = 12.f;
        click_tone_ = 0.3f;
        
        // Filter Parameter
        filter_enabled_ = true;
        filter_cutoff_ = 0.95f;
        filter_resonance_ = 0.3f;
        filter_mode_24db_ = false;
        
        osc2_enabled_ = 1;
        osc2_waveform_ = k_wave_noise;
        osc2_pitch_ = 1.0f;
        osc2_level_ = 0.7f;
        fm_amount_ = 0.0f;
        fm_ratio_ = 1.0f;
        osc2_decay_ = 20.f;
        break;

      default:
        break;
    }
  }

  inline uint8_t getPresetIndex() const {
    return preset_index_;
  }

  /*===========================================================================*/
  /* Static Members. */
  /*===========================================================================*/

  static inline const char * getPresetName(uint8_t idx) {
    if (idx < 5) {
      return s_preset_names[idx];
    }
    return "---";
  }

private:
  /*===========================================================================*/
  /* Private Member Variables. */
  /*===========================================================================*/
  
  // Parameter-Indizes
  enum {
    k_param_pitch = 0,          // Grundfrequenz der Kick
    k_param_decay,              // Abklingzeit des Pitch-Envelopes
    k_param_body_level,         // Lautstärke des Hauptoszillators
    k_param_drive,              // Verzerrung/Drive
    k_param_attack,             // Anstiegszeit
    k_param_release,            // Ausklingzeit
    k_param_pitch_curve,        // Verlauf des Pitch-Envelopes
    
    // Neue Click-Parameter
    k_param_click_level,        // Stärke des Anschlagsklicks
    k_param_click_freq,         // Frequenz des Klicks
    k_param_click_decay,        // Abklingzeit des Klicks
    k_param_click_tone,         // Tonalität des Klicks (0:Noise, 1:Tonal)
    
    // Neue Filter-Parameter
    k_param_filter_enabled,     // Filter an/aus
    k_param_filter_cutoff,      // Cutoff-Frequenz
    k_param_filter_resonance,   // Resonanz
    k_param_filter_mode,        // 12dB/24dB-Modus
    
    // Parameter für den zweiten Oszillator
    k_param_osc2_enabled,       // Zweiter Oszillator an/aus
    k_param_osc2_waveform,      // Wellenform des zweiten Oszillators
    k_param_osc2_pitch,         // Tonhöhe des zweiten Oszillators relativ zum ersten
    k_param_osc2_level,         // Lautstärke des zweiten Oszillators
    k_param_fm_amount,          // Stärke der Frequenzmodulation
    k_param_fm_ratio,           // Verhältnis der FM-Modulationsfrequenz
    k_param_osc2_decay          // Separate Abklingzeit für den zweiten Oszillator
  };
  
  // Preset-Indizes
  enum {
    k_preset_basic = 0,
    k_preset_punchy,
    k_preset_sub,
    k_preset_fm_kick,
    k_preset_noise_attack
  };
  
  enum {
    k_state_off = 0,
    k_state_attack,
    k_state_decay,
    k_state_release
  };

  // Oszillator und Envelope Zustände
  float phase1_;           // Phase des Hauptoszillators
  float phase2_;           // Phase des zweiten Oszillators
  float click_phase_;      // Phase des Click-Oszillators (neu)
  float envelope_;         // Amplituden-Envelope
  float pitch_envelope_;   // Pitch-Envelope für Oszillator 1
  float osc2_envelope_;    // Separates Envelope für Oszillator 2
  float click_envelope_;   // Separates Envelope für Click (neu)
  int envelope_state_;
  
  uint8_t current_note_;
  float current_velocity_;
  uint8_t preset_index_;
  
  // Haupt-Parameter
  float pitch_;
  float decay_;
  float pitch_curve_;
  float body_level_;
  float drive_;
  float attack_;
  float release_;
  
  // Click-Parameter (neu)
  float click_level_;
  float click_freq_;
  float click_decay_;
  float click_tone_;
  float last_noise_;
  
  // Filter-Parameter (neu)
  bool filter_enabled_;
  float filter_cutoff_;
  float filter_resonance_;
  bool filter_mode_24db_;
  float filter_state_[4];  // Für einen 4-Pol Filter (24dB/Okt)
  
  // Oszillator 2 Parameter
  uint8_t osc2_enabled_;
  uint8_t osc2_waveform_;
  float osc2_pitch_;
  float osc2_level_;
  float fm_amount_;
  float fm_ratio_;
  float osc2_decay_;
  float pulse_width_; // Pulsbreite für Pulse-Wellenform
  
  std::atomic_uint_fast32_t flags_;
};
