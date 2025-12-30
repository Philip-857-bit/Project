import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:equatable/equatable.dart';
import 'package:local_auth/local_auth.dart';

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
  final bool biometricAvailable;
  final bool biometricEnabled;

  const AuthState({
    this.isAuthenticated = false,
    this.isLoading = false,
    this.userId,
    this.userName,
    this.email,
    this.error,
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
      biometricAvailable: biometricAvailable ?? this.biometricAvailable,
      biometricEnabled: biometricEnabled ?? this.biometricEnabled,
    );
  }

  @override
  List<Object?> get props => [isAuthenticated, isLoading, userId, userName, email, error, biometricAvailable, biometricEnabled];
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
        localizedReason: 'Authenticate to access Smart Fish Feeder',
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
          state = state.copyWith(
            isAuthenticated: true,
            userId: userId,
          );
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
    state = state.copyWith(isLoading: true, error: null);

    try {
      final response = await _apiService.login(email, password);
      
      if (response.statusCode == 200) {
        final data = response.data;
        await StorageService.setAccessToken(data['access_token']);
        await StorageService.setRefreshToken(data['refresh_token']);
        await StorageService.setUserId(data['user']['id']);

        state = state.copyWith(
          isAuthenticated: true,
          isLoading: false,
          userId: data['user']['id'],
          userName: data['user']['name'],
          email: data['user']['email'],
        );
        return true;
      } else {
        state = state.copyWith(
          isLoading: false,
          error: 'Login failed. Please check your credentials.',
        );
        return false;
      }
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        error: 'An error occurred. Please try again.',
      );
      return false;
    }
  }

  Future<bool> register(String name, String email, String password) async {
    state = state.copyWith(isLoading: true, error: null);

    try {
      final response = await _apiService.register(name, email, password);
      
      if (response.statusCode == 201) {
        // Auto-login after registration
        return await login(email, password);
      } else {
        state = state.copyWith(
          isLoading: false,
          error: response.data['message'] ?? 'Registration failed.',
        );
        return false;
      }
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        error: 'An error occurred. Please try again.',
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
    state = state.copyWith(error: null);
  }

  // Password Reset
  Future<bool> requestPasswordReset(String email) async {
    state = state.copyWith(isLoading: true, error: null);

    try {
      final response = await _apiService.requestPasswordReset(email);
      state = state.copyWith(isLoading: false);
      return response.statusCode == 200;
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        error: 'Failed to send reset email. Please try again.',
      );
      return false;
    }
  }

  Future<bool> verifyResetCode(String email, String code) async {
    state = state.copyWith(isLoading: true, error: null);

    try {
      final response = await _apiService.verifyResetCode(email, code);
      state = state.copyWith(isLoading: false);
      return response.statusCode == 200;
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        error: 'Invalid or expired code.',
      );
      return false;
    }
  }

  Future<bool> resetPassword(String email, String code, String newPassword) async {
    state = state.copyWith(isLoading: true, error: null);

    try {
      final response = await _apiService.resetPassword(email, code, newPassword);
      state = state.copyWith(isLoading: false);
      return response.statusCode == 200;
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        error: 'Failed to reset password. Please try again.',
      );
      return false;
    }
  }
}

// Providers
final apiServiceProvider = Provider<ApiService>((ref) => ApiService());

final authStateProvider = StateNotifierProvider<AuthNotifier, AuthState>((ref) {
  final apiService = ref.watch(apiServiceProvider);
  return AuthNotifier(apiService);
});
