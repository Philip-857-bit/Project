import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:equatable/equatable.dart';
import 'package:local_auth/local_auth.dart';
import 'package:dio/dio.dart';

import '../services/api_service.dart';
import '../services/storage_service.dart';

// Auth State
class AuthState extends Equatable {
  final bool isAuthenticated;
  final bool isLoading;
  final String? userId;
  final String? userName;
  final String? email;
  final String? error;
  final String? statusMessage;
  final bool biometricAvailable;
  final bool biometricEnabled;

  const AuthState({
    this.isAuthenticated = false,
    this.isLoading = false,
    this.userId,
    this.userName,
    this.email,
    this.error,
    this.statusMessage,
    this.biometricAvailable = false,
    this.biometricEnabled = false,
  });

  AuthState copyWith({
    bool? isAuthenticated,
    bool? isLoading,
    String? userId,
    String? userName,
    String? email,
    String? error,
    String? statusMessage,
    bool? biometricAvailable,
    bool? biometricEnabled,
  }) {
    return AuthState(
      isAuthenticated: isAuthenticated ?? this.isAuthenticated,
      isLoading: isLoading ?? this.isLoading,
      userId: userId ?? this.userId,
      userName: userName ?? this.userName,
      email: email ?? this.email,
      error: error,
      statusMessage: statusMessage,
      biometricAvailable: biometricAvailable ?? this.biometricAvailable,
      biometricEnabled: biometricEnabled ?? this.biometricEnabled,
    );
  }

  @override
  List<Object?> get props => [
    isAuthenticated,
    isLoading,
    userId,
    userName,
    email,
    error,
    statusMessage,
    biometricAvailable,
    biometricEnabled,
  ];
}

// Auth Notifier
class AuthNotifier extends StateNotifier<AuthState> {
  final ApiService _apiService;
  final LocalAuthentication _localAuth = LocalAuthentication();

  AuthNotifier(this._apiService) : super(const AuthState()) {
    _checkBiometricAvailability();
  }

  Future<void> _checkBiometricAvailability() async {
    try {
      final canCheckBiometrics = await _localAuth.canCheckBiometrics;
      final isDeviceSupported = await _localAuth.isDeviceSupported();
      final biometricEnabled = StorageService.getBiometricEnabled();

      state = state.copyWith(
        biometricAvailable: canCheckBiometrics && isDeviceSupported,
        biometricEnabled: biometricEnabled,
      );
    } catch (e) {
      state = state.copyWith(biometricAvailable: false);
    }
  }

  Future<bool> authenticateWithBiometrics() async {
    if (!state.biometricAvailable || !state.biometricEnabled) {
      return false;
    }

    try {
      final authenticated = await _localAuth.authenticate(
        localizedReason: 'Authenticate to access SmartAqua',
        options: const AuthenticationOptions(
          stickyAuth: true,
          biometricOnly: true,
        ),
      );

      if (authenticated) {
        // Check if we have stored credentials
        final token = await StorageService.getAccessToken();
        if (token != null) {
          final userId = StorageService.getUserId();
          state = state.copyWith(isAuthenticated: true, userId: userId);
          return true;
        }
      }
      return false;
    } catch (e) {
      return false;
    }
  }

  Future<void> setBiometricEnabled(bool enabled) async {
    await StorageService.setBiometricEnabled(enabled);
    state = state.copyWith(biometricEnabled: enabled);
  }

  Future<List<BiometricType>> getAvailableBiometrics() async {
    try {
      return await _localAuth.getAvailableBiometrics();
    } catch (e) {
      return [];
    }
  }

  Future<void> checkAuthStatus() async {
    state = state.copyWith(isLoading: true);

    try {
      final token = await StorageService.getAccessToken();
      if (token != null) {
        final userId = StorageService.getUserId();
        state = state.copyWith(
          isAuthenticated: true,
          isLoading: false,
          userId: userId,
        );
      } else {
        state = state.copyWith(isAuthenticated: false, isLoading: false);
      }
    } catch (e) {
      state = state.copyWith(isAuthenticated: false, isLoading: false);
    }
  }

  Future<bool> login(String email, String password) async {
    state = state.copyWith(isLoading: true, error: null, statusMessage: null);

    try {
      final response = await _apiService.login(email, password);

      if (response.statusCode == 200) {
        final data = response.data;
        await StorageService.setAccessToken(data['access_token']);
        await StorageService.setRefreshToken(data['refresh_token']);

        String? userId;
        String? userName;
        String? profileEmail = email;

        try {
          final profileResponse = await _apiService.getProfile();
          final user = profileResponse.data['user'] ?? profileResponse.data;

          userId = user['id']?.toString();
          final firstName = user['first_name']?.toString().trim() ?? '';
          final lastName = user['last_name']?.toString().trim() ?? '';
          userName =
              [
                firstName,
                lastName,
              ].where((part) => part.isNotEmpty).join(' ').trim();
          profileEmail = user['email']?.toString() ?? email;

          if (userId != null && userId.isNotEmpty) {
            await StorageService.setUserId(userId);
          }
        } catch (_) {
          userId = StorageService.getUserId();
        }

        state = state.copyWith(
          isAuthenticated: true,
          isLoading: false,
          userId: userId,
          userName: userName,
          email: profileEmail,
          statusMessage: null,
        );
        return true;
      } else {
        state = state.copyWith(
          isLoading: false,
          error: 'Login failed. Please check your credentials.',
          statusMessage: null,
        );
        return false;
      }
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        error: _extractErrorMessage(
          e,
          fallback: 'Login failed. Please try again.',
        ),
        statusMessage: null,
      );
      return false;
    }
  }

  Future<bool> register(String name, String email, String password) async {
    state = state.copyWith(isLoading: true, error: null, statusMessage: null);

    try {
      final response = await _apiService.register(name, email, password);

      if (response.statusCode == 201) {
        // Auto-login after registration
        return await login(email, password);
      } else {
        state = state.copyWith(
          isLoading: false,
          error: response.data['message'] ?? 'Registration failed.',
          statusMessage: null,
        );
        return false;
      }
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        error: _extractErrorMessage(
          e,
          fallback: 'Registration failed. Please try again.',
        ),
        statusMessage: null,
      );
      return false;
    }
  }

  Future<void> logout() async {
    try {
      await _apiService.logout();
    } catch (_) {}

    await StorageService.clearTokens();
    state = const AuthState();
  }

  void clearError() {
    state = state.copyWith(error: null, statusMessage: null);
  }

  // Password Reset
  Future<bool> requestPasswordReset(String email) async {
    state = state.copyWith(isLoading: true, error: null, statusMessage: null);

    try {
      final response = await _apiService.requestPasswordReset(email);
      final data = response.data;
      final resetCode =
          data is Map<String, dynamic> ? data['reset_code']?.toString() : null;
      final message =
          data is Map<String, dynamic> ? data['message']?.toString() : null;

      state = state.copyWith(
        isLoading: false,
        error: null,
        statusMessage:
            resetCode != null && resetCode.isNotEmpty
                ? 'Use reset code $resetCode to continue.'
                : (message ?? 'Reset request accepted.'),
      );
      return response.statusCode == 200;
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        error: _extractErrorMessage(
          e,
          fallback: 'Failed to request password reset.',
        ),
        statusMessage: null,
      );
      return false;
    }
  }

  Future<bool> verifyResetCode(String email, String code) async {
    state = state.copyWith(isLoading: true, error: null, statusMessage: null);

    try {
      final response = await _apiService.verifyResetCode(email, code);
      final data = response.data;
      final message =
          data is Map<String, dynamic> ? data['message']?.toString() : null;

      state = state.copyWith(
        isLoading: false,
        error: null,
        statusMessage: message ?? 'Reset code verified.',
      );
      return response.statusCode == 200;
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        error: _extractErrorMessage(
          e,
          fallback: 'Failed to verify reset code.',
        ),
        statusMessage: null,
      );
      return false;
    }
  }

  Future<bool> resetPassword(
    String email,
    String code,
    String newPassword,
  ) async {
    state = state.copyWith(isLoading: true, error: null, statusMessage: null);

    try {
      final response = await _apiService.resetPassword(
        email,
        code,
        newPassword,
      );
      final data = response.data;
      final message =
          data is Map<String, dynamic> ? data['message']?.toString() : null;

      state = state.copyWith(
        isLoading: false,
        error: null,
        statusMessage: message ?? 'Password reset successfully.',
      );
      return response.statusCode == 200;
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        error: _extractErrorMessage(e, fallback: 'Failed to reset password.'),
        statusMessage: null,
      );
      return false;
    }
  }

  String _extractErrorMessage(Object error, {required String fallback}) {
    if (error is DioException) {
      final data = error.response?.data;
      if (data is Map<String, dynamic>) {
        final message = data['error'] ?? data['message'] ?? data['details'];
        if (message is String && message.isNotEmpty) {
          return message;
        }
      }
      if (error.message != null && error.message!.isNotEmpty) {
        return error.message!;
      }
    }

    final message = error.toString();
    if (message.isNotEmpty && message != 'Exception') {
      return message;
    }

    return fallback;
  }
}

// Providers
final apiServiceProvider = Provider<ApiService>((ref) => ApiService());

final authStateProvider = StateNotifierProvider<AuthNotifier, AuthState>((ref) {
  final apiService = ref.watch(apiServiceProvider);
  return AuthNotifier(apiService);
});
