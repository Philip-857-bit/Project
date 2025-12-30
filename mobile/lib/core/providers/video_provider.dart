import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/video_verification.dart';
import '../services/api_service.dart';
import 'auth_provider.dart';

// Video Verification State
class VideoVerificationState {
  final FeedingVerification? verification;
  final List<VideoClip> recentClips;
  final bool isLoading;
  final String? error;

  const VideoVerificationState({
    this.verification,
    this.recentClips = const [],
    this.isLoading = false,
    this.error,
  });

  VideoVerificationState copyWith({
    FeedingVerification? verification,
    List<VideoClip>? recentClips,
    bool? isLoading,
    String? error,
  }) {
    return VideoVerificationState(
      verification: verification ?? this.verification,
      recentClips: recentClips ?? this.recentClips,
      isLoading: isLoading ?? this.isLoading,
      error: error,
    );
  }
}

// Video Verification Notifier
class VideoVerificationNotifier extends StateNotifier<VideoVerificationState> {
  final ApiService _apiService;

  VideoVerificationNotifier(this._apiService) : super(const VideoVerificationState());

  Future<void> loadVerification(String feedingEventId) async {
    state = state.copyWith(isLoading: true, error: null);

    try {
      final response = await _apiService.dio.get('/feeding-events/$feedingEventId/verification');
      if (response.statusCode == 200) {
        final verification = FeedingVerification.fromJson(response.data);
        state = state.copyWith(verification: verification, isLoading: false);
      } else {
        state = state.copyWith(isLoading: false, error: 'Failed to load verification');
      }
    } catch (e) {
      state = state.copyWith(isLoading: false, error: e.toString());
    }
  }

  Future<void> loadRecentClips(String deviceId, {int limit = 10}) async {
    state = state.copyWith(isLoading: true, error: null);

    try {
      final response = await _apiService.dio.get(
        '/devices/$deviceId/video-clips',
        queryParameters: {'limit': limit},
      );
      if (response.statusCode == 200) {
        final List<dynamic> data = response.data['clips'] ?? response.data ?? [];
        final clips = data.map((c) => VideoClip.fromJson(c)).toList();
        state = state.copyWith(recentClips: clips, isLoading: false);
      } else {
        state = state.copyWith(isLoading: false, error: 'Failed to load clips');
      }
    } catch (e) {
      state = state.copyWith(isLoading: false, error: e.toString());
    }
  }

  Future<bool> requestVideoCapture(String deviceId) async {
    try {
      final response = await _apiService.dio.post('/devices/$deviceId/capture-video');
      return response.statusCode == 200;
    } catch (e) {
      return false;
    }
  }

  void clearVerification() {
    state = state.copyWith(verification: null);
  }
}

// Providers
final videoVerificationProvider = StateNotifierProvider<VideoVerificationNotifier, VideoVerificationState>((ref) {
  final apiService = ref.watch(apiServiceProvider);
  return VideoVerificationNotifier(apiService);
});

// Convenience providers
final currentVerificationProvider = Provider<FeedingVerification?>((ref) {
  return ref.watch(videoVerificationProvider).verification;
});

final recentClipsProvider = Provider<List<VideoClip>>((ref) {
  return ref.watch(videoVerificationProvider).recentClips;
});
