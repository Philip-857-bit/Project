/// Environment configuration for the Smart Fish Feeder mobile app.
/// 
/// Values are loaded from compile-time environment variables using --dart-define.
/// 
/// Build commands:
/// ```bash
/// # Development
/// flutter build apk --dart-define-from-file=.env
/// 
/// # Or individual defines
/// flutter build apk \
///   --dart-define=API_BASE_URL=https://api.smartfishfeeder.com/api/v1 \
///   --dart-define=MQTT_HOST=mqtt.smartfishfeeder.com \
///   --dart-define=MQTT_PORT=8883
/// ```
class EnvConfig {
  // Private constructor
  EnvConfig._();

  /// API base URL
  static const String apiBaseUrl = String.fromEnvironment(
    'API_BASE_URL',
    defaultValue: 'https://api.smartfishfeeder.com/api/v1',
  );

  /// MQTT broker host
  static const String mqttHost = String.fromEnvironment(
    'MQTT_HOST',
    defaultValue: 'mqtt.smartfishfeeder.com',
  );

  /// MQTT broker port
  static const int mqttPort = int.fromEnvironment(
    'MQTT_PORT',
    defaultValue: 8883,
  );

  /// Primary certificate fingerprint for TLS pinning
  static const String certFingerprint1 = String.fromEnvironment(
    'CERT_FINGERPRINT_1',
    defaultValue: '',
  );

  /// Backup certificate fingerprint for TLS pinning
  static const String certFingerprint2 = String.fromEnvironment(
    'CERT_FINGERPRINT_2',
    defaultValue: '',
  );

  /// API domain for certificate validation
  static const String apiDomain = String.fromEnvironment(
    'API_DOMAIN',
    defaultValue: 'smartfishfeeder.com',
  );

  /// Debug mode flag
  static const bool debugMode = bool.fromEnvironment(
    'DEBUG_MODE',
    defaultValue: false,
  );

  /// Check if running in development mode
  static bool get isDevelopment => 
      const bool.fromEnvironment('dart.vm.product') == false;

  /// Get list of pinned certificate fingerprints
  static List<String> get pinnedCertificates {
    final certs = <String>[];
    if (certFingerprint1.isNotEmpty) {
      certs.add('sha256/$certFingerprint1');
    }
    if (certFingerprint2.isNotEmpty) {
      certs.add('sha256/$certFingerprint2');
    }
    return certs;
  }
}
