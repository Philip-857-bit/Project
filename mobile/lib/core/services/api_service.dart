import 'dart:convert';
import 'dart:io';
import 'package:crypto/crypto.dart';
import 'package:dio/dio.dart';
import 'package:dio/io.dart';
import 'package:logger/logger.dart';

import '../config/env_config.dart';
import 'storage_service.dart';

class ApiService {
  /// API base URL from environment configuration
  static String get baseUrl => EnvConfig.apiBaseUrl;
  
  /// Certificate fingerprints from environment configuration
  static List<String> get _pinnedCertificates => EnvConfig.pinnedCertificates;
  
  /// API domain for certificate validation
  static String get _apiDomain => EnvConfig.apiDomain;
  
  late final Dio _dio;
  final Logger _logger = Logger();

  ApiService() {
    _dio = Dio(BaseOptions(
      baseUrl: baseUrl,
      connectTimeout: const Duration(seconds: 30),
      receiveTimeout: const Duration(seconds: 30),
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
      },
    ));

    // Configure certificate pinning
    _configureCertificatePinning();

    _dio.interceptors.add(InterceptorsWrapper(
      onRequest: _onRequest,
      onResponse: _onResponse,
      onError: _onError,
    ));
  }

  /// Configure certificate pinning for enhanced security
  void _configureCertificatePinning() {
    (_dio.httpClientAdapter as IOHttpClientAdapter).createHttpClient = () {
      final client = HttpClient();
      client.badCertificateCallback = (X509Certificate cert, String host, int port) {
        // Only validate certificates for our domain
        if (!host.contains(_apiDomain)) {
          return false;
        }
        
        // Skip pinning if no certificates configured
        if (_pinnedCertificates.isEmpty) {
          if (EnvConfig.isDevelopment) {
            _logger.w('No certificate fingerprints configured - skipping pinning in dev mode');
            return true;
          }
          _logger.e('No certificate fingerprints configured!');
          return false;
        }
        
        // Compute SHA256 fingerprint of the certificate
        final fingerprint = _computeCertificateFingerprint(cert);
        
        // Check if fingerprint matches any pinned certificate
        for (final pinned in _pinnedCertificates) {
          if (pinned.contains(fingerprint)) {
            return true;
          }
        }
        
        // In development mode, allow self-signed certificates
        if (EnvConfig.isDevelopment || EnvConfig.debugMode) {
          _logger.w('Certificate pinning bypassed in development mode');
          return true;
        }
        
        _logger.e('Certificate pinning failed for $host');
        return false;
      };
      return client;
    };
  }

  /// Compute SHA256 fingerprint of X509 certificate
  String _computeCertificateFingerprint(X509Certificate cert) {
    // Get the DER-encoded certificate bytes
    final derBytes = cert.der;
    // Compute SHA256 hash
    final digest = sha256.convert(derBytes);
    // Return base64-encoded fingerprint
    return base64.encode(digest.bytes);
  }

  Future<void> _onRequest(
    RequestOptions options,
    RequestInterceptorHandler handler,
  ) async {
    final token = await StorageService.getAccessToken();
    if (token != null) {
      options.headers['Authorization'] = 'Bearer $token';
    }
    _logger.d('REQUEST[${options.method}] => PATH: ${options.path}');
    handler.next(options);
  }

  void _onResponse(Response response, ResponseInterceptorHandler handler) {
    _logger.d('RESPONSE[${response.statusCode}] => PATH: ${response.requestOptions.path}');
    handler.next(response);
  }

  Future<void> _onError(DioException err, ErrorInterceptorHandler handler) async {
    _logger.e('ERROR[${err.response?.statusCode}] => PATH: ${err.requestOptions.path}');
    
    if (err.response?.statusCode == 401) {
      // Try to refresh token
      final refreshed = await _refreshToken();
      if (refreshed) {
        // Retry the request
        final response = await _retry(err.requestOptions);
        handler.resolve(response);
        return;
      }
    }
    
    handler.next(err);
  }

  Future<bool> _refreshToken() async {
    try {
      final refreshToken = await StorageService.getRefreshToken();
      if (refreshToken == null) return false;

      final response = await Dio().post(
        '$baseUrl/auth/refresh',
        data: {'refresh_token': refreshToken},
      );

      if (response.statusCode == 200) {
        final newAccessToken = response.data['access_token'];
        final newRefreshToken = response.data['refresh_token'];
        
        await StorageService.setAccessToken(newAccessToken);
        await StorageService.setRefreshToken(newRefreshToken);
        
        return true;
      }
    } catch (e) {
      _logger.e('Token refresh failed: $e');
    }
    return false;
  }

  Future<Response> _retry(RequestOptions requestOptions) async {
    final token = await StorageService.getAccessToken();
    final options = Options(
      method: requestOptions.method,
      headers: {
        ...requestOptions.headers,
        'Authorization': 'Bearer $token',
      },
    );
    return _dio.request(
      requestOptions.path,
      data: requestOptions.data,
      queryParameters: requestOptions.queryParameters,
      options: options,
    );
  }

  Dio get dio => _dio;

  // Auth endpoints
  Future<Response> login(String email, String password) async {
    return _dio.post('/auth/login', data: {
      'email': email,
      'password': password,
    });
  }

  Future<Response> register(String name, String email, String password) async {
    return _dio.post('/auth/register', data: {
      'name': name,
      'email': email,
      'password': password,
    });
  }

  Future<Response> logout() async {
    return _dio.post('/auth/logout');
  }

  // Device endpoints
  Future<Response> getDevices() async {
    return _dio.get('/devices');
  }

  Future<Response> getDevice(String deviceId) async {
    return _dio.get('/devices/$deviceId');
  }

  Future<Response> bindDevice(String bindingCode) async {
    return _dio.post('/devices/bind', data: {'binding_code': bindingCode});
  }

  Future<Response> unbindDevice(String deviceId) async {
    return _dio.delete('/devices/$deviceId/unbind');
  }

  // Feeding endpoints
  Future<Response> getSchedules(String deviceId) async {
    return _dio.get('/devices/$deviceId/schedules');
  }

  Future<Response> createSchedule(String deviceId, Map<String, dynamic> schedule) async {
    return _dio.post('/devices/$deviceId/schedules', data: schedule);
  }

  Future<Response> updateSchedule(String deviceId, String scheduleId, Map<String, dynamic> schedule) async {
    return _dio.put('/devices/$deviceId/schedules/$scheduleId', data: schedule);
  }

  Future<Response> deleteSchedule(String deviceId, String scheduleId) async {
    return _dio.delete('/devices/$deviceId/schedules/$scheduleId');
  }

  Future<Response> triggerManualFeed(String deviceId, double amount) async {
    return _dio.post('/devices/$deviceId/feed', data: {'amount': amount});
  }

  Future<Response> getFeedingHistory(String deviceId, {int? limit, int? offset}) async {
    return _dio.get('/devices/$deviceId/feeding-history', queryParameters: {
      if (limit != null) 'limit': limit,
      if (offset != null) 'offset': offset,
    });
  }

  // Monitoring endpoints
  Future<Response> getSensorData(String deviceId) async {
    return _dio.get('/devices/$deviceId/sensors');
  }

  Future<Response> getSensorHistory(String deviceId, String sensorType, {int? hours}) async {
    return _dio.get('/devices/$deviceId/sensors/$sensorType/history', queryParameters: {
      if (hours != null) 'hours': hours,
    });
  }

  Future<Response> getAlerts(String deviceId) async {
    return _dio.get('/devices/$deviceId/alerts');
  }

  // Calculator endpoints
  Future<Response> calculateFeed(Map<String, dynamic> params) async {
    return _dio.post('/calculator/feed', data: params);
  }

  Future<Response> getSpecies() async {
    return _dio.get('/calculator/species');
  }

  // Password reset endpoints
  Future<Response> requestPasswordReset(String email) async {
    return _dio.post('/auth/password-reset/request', data: {'email': email});
  }

  Future<Response> verifyResetCode(String email, String code) async {
    return _dio.post('/auth/password-reset/verify', data: {
      'email': email,
      'code': code,
    });
  }

  Future<Response> resetPassword(String email, String code, String newPassword) async {
    return _dio.post('/auth/password-reset/confirm', data: {
      'email': email,
      'code': code,
      'new_password': newPassword,
    });
  }

  // Video verification endpoints
  Future<Response> getVideoClips(String deviceId, {int? limit}) async {
    return _dio.get('/devices/$deviceId/videos', queryParameters: {
      if (limit != null) 'limit': limit,
    });
  }

  Future<Response> getVideoVerification(String feedingEventId) async {
    return _dio.get('/feeding-events/$feedingEventId/verification');
  }

  Future<Response> requestVideoCapture(String deviceId) async {
    return _dio.post('/devices/$deviceId/capture-video');
  }

  // FCR Analytics endpoints
  Future<Response> getFCRAnalytics(String deviceId, {int? days}) async {
    return _dio.get('/devices/$deviceId/fcr-analytics', queryParameters: {
      if (days != null) 'days': days,
    });
  }

  Future<Response> getGrowthPrediction(String deviceId) async {
    return _dio.get('/devices/$deviceId/growth-prediction');
  }
}
