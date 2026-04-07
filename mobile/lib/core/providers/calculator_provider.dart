import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:logger/logger.dart';

import '../models/feeding.dart';
import '../services/api_service.dart';
import 'auth_provider.dart';

// Species List State
class SpeciesListState {
  final List<FishSpecies> species;
  final bool isLoading;
  final String? error;
  final DateTime? lastRequestAt;
  final DateTime? lastSuccessAt;
  final int? lastStatusCode;

  const SpeciesListState({
    this.species = const [],
    this.isLoading = false,
    this.error,
    this.lastRequestAt,
    this.lastSuccessAt,
    this.lastStatusCode,
  });

  SpeciesListState copyWith({
    List<FishSpecies>? species,
    bool? isLoading,
    String? error,
    DateTime? lastRequestAt,
    DateTime? lastSuccessAt,
    int? lastStatusCode,
  }) {
    return SpeciesListState(
      species: species ?? this.species,
      isLoading: isLoading ?? this.isLoading,
      error: error,
      lastRequestAt: lastRequestAt ?? this.lastRequestAt,
      lastSuccessAt: lastSuccessAt ?? this.lastSuccessAt,
      lastStatusCode: lastStatusCode ?? this.lastStatusCode,
    );
  }
}

// Species List Notifier
class SpeciesListNotifier extends StateNotifier<SpeciesListState> {
  final ApiService _apiService;
  final Logger _logger = Logger();

  SpeciesListNotifier(this._apiService) : super(const SpeciesListState());

  Future<void> loadSpecies() async {
    final startedAt = DateTime.now();
    state = state.copyWith(
      isLoading: true,
      error: null,
      lastRequestAt: startedAt,
    );
    _logger.i('Species load start @ ${startedAt.toIso8601String()}');

    try {
      final response = await _apiService.getSpecies().timeout(
        const Duration(seconds: 20),
      );
      final status = response.statusCode ?? 0;
      if (status == 200) {
        final List<dynamic> data =
            response.data['species'] ?? response.data ?? [];
        final species = data.map((json) => FishSpecies.fromJson(json)).toList();
        state = state.copyWith(
          species: species,
          isLoading: false,
          lastSuccessAt: DateTime.now(),
          lastStatusCode: status,
        );
        _logger.i('Species load success (count=${species.length})');
      } else {
        state = state.copyWith(
          isLoading: false,
          error: 'Failed to load species',
          lastStatusCode: status,
        );
        _logger.w('Species load failed (status=$status)');
      }
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        error: ApiService.describeError(e, fallback: 'Failed to load species.'),
      );
      _logger.e('Species load error: $e');
    }
  }
}

// Calculator State
class CalculatorState {
  final FeedCalculationResult? result;
  final bool isCalculating;
  final String? error;
  final List<FeedCalculationResult> history;

  const CalculatorState({
    this.result,
    this.isCalculating = false,
    this.error,
    this.history = const [],
  });

  CalculatorState copyWith({
    FeedCalculationResult? result,
    bool? isCalculating,
    String? error,
    List<FeedCalculationResult>? history,
  }) {
    return CalculatorState(
      result: result ?? this.result,
      isCalculating: isCalculating ?? this.isCalculating,
      error: error,
      history: history ?? this.history,
    );
  }
}

// Calculator Notifier
class CalculatorNotifier extends StateNotifier<CalculatorState> {
  final ApiService _apiService;

  CalculatorNotifier(this._apiService) : super(const CalculatorState());

  Future<FeedCalculationResult?> calculate(
    FeedCalculationRequest request,
  ) async {
    state = state.copyWith(isCalculating: true, error: null);

    try {
      final response = await _apiService
          .calculateFeed(request.toJson())
          .timeout(const Duration(seconds: 20));
      if (response.statusCode == 200) {
        final result = FeedCalculationResult.fromJson(response.data);
        state = state.copyWith(
          result: result,
          isCalculating: false,
          history: [result, ...state.history.take(9)],
        );
        return result;
      } else {
        state = state.copyWith(
          isCalculating: false,
          error: response.data['message'] ?? 'Calculation failed',
        );
        return null;
      }
    } catch (e) {
      state = state.copyWith(
        isCalculating: false,
        error: ApiService.describeError(e, fallback: 'Calculation failed.'),
      );
      return null;
    }
  }

  void clearResult() {
    state = state.copyWith(result: null, error: null);
  }

  void clearHistory() {
    state = state.copyWith(history: []);
  }
}

// Providers
final speciesListProvider =
    StateNotifierProvider<SpeciesListNotifier, SpeciesListState>((ref) {
      final apiService = ref.watch(apiServiceProvider);
      return SpeciesListNotifier(apiService);
    });

final calculatorProvider =
    StateNotifierProvider<CalculatorNotifier, CalculatorState>((ref) {
      final apiService = ref.watch(apiServiceProvider);
      return CalculatorNotifier(apiService);
    });

// Convenience providers
final speciesProvider = Provider<List<FishSpecies>>((ref) {
  return ref.watch(speciesListProvider).species;
});

final speciesByIdProvider = Provider.family<FishSpecies?, String>((
  ref,
  speciesId,
) {
  final species = ref.watch(speciesProvider);
  try {
    return species.firstWhere((s) => s.id == speciesId);
  } catch (_) {
    return null;
  }
});

final calculationResultProvider = Provider<FeedCalculationResult?>((ref) {
  return ref.watch(calculatorProvider).result;
});
