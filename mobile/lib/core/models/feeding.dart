import 'package:equatable/equatable.dart';

class FeedingSchedule extends Equatable {
  final String id;
  final String deviceId;
  final String time;
  final double amount;
  final List<int> daysOfWeek;
  final bool isEnabled;
  final DateTime? createdAt;
  final DateTime? updatedAt;

  const FeedingSchedule({
    required this.id,
    required this.deviceId,
    required this.time,
    required this.amount,
    required this.daysOfWeek,
    required this.isEnabled,
    this.createdAt,
    this.updatedAt,
  });

  factory FeedingSchedule.fromJson(Map<String, dynamic> json) {
    return FeedingSchedule(
      id: json['id'] ?? '',
      deviceId: json['device_id'] ?? '',
      time: json['time'] ?? '08:00',
      amount: (json['amount'] ?? 0).toDouble(),
      daysOfWeek: List<int>.from(json['days_of_week'] ?? [0, 1, 2, 3, 4, 5, 6]),
      isEnabled: json['is_enabled'] ?? true,
      createdAt: json['created_at'] != null ? DateTime.parse(json['created_at']) : null,
      updatedAt: json['updated_at'] != null ? DateTime.parse(json['updated_at']) : null,
    );
  }

  Map<String, dynamic> toJson() => {
    'id': id,
    'device_id': deviceId,
    'time': time,
    'amount': amount,
    'days_of_week': daysOfWeek,
    'is_enabled': isEnabled,
  };

  FeedingSchedule copyWith({
    String? id,
    String? deviceId,
    String? time,
    double? amount,
    List<int>? daysOfWeek,
    bool? isEnabled,
  }) {
    return FeedingSchedule(
      id: id ?? this.id,
      deviceId: deviceId ?? this.deviceId,
      time: time ?? this.time,
      amount: amount ?? this.amount,
      daysOfWeek: daysOfWeek ?? this.daysOfWeek,
      isEnabled: isEnabled ?? this.isEnabled,
      createdAt: createdAt,
      updatedAt: updatedAt,
    );
  }

  String get daysDescription {
    if (daysOfWeek.length == 7) return 'Every day';
    if (daysOfWeek.length == 5 && !daysOfWeek.contains(0) && !daysOfWeek.contains(6)) {
      return 'Weekdays';
    }
    if (daysOfWeek.length == 2 && daysOfWeek.contains(0) && daysOfWeek.contains(6)) {
      return 'Weekends';
    }
    final dayNames = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
    return daysOfWeek.map((d) => dayNames[d]).join(', ');
  }

  @override
  List<Object?> get props => [id, deviceId, time, amount, daysOfWeek, isEnabled];
}

enum FeedingEventStatus { completed, failed, pending, cancelled }

class FeedingEvent extends Equatable {
  final String id;
  final String deviceId;
  final double amount;
  final double? actualAmount;
  final FeedingEventStatus status;
  final String type;
  final String? errorMessage;
  final DateTime scheduledAt;
  final DateTime? completedAt;
  final double? waterTemperature;
  final double? dissolvedOxygen;

  const FeedingEvent({
    required this.id,
    required this.deviceId,
    required this.amount,
    this.actualAmount,
    required this.status,
    required this.type,
    this.errorMessage,
    required this.scheduledAt,
    this.completedAt,
    this.waterTemperature,
    this.dissolvedOxygen,
  });

  factory FeedingEvent.fromJson(Map<String, dynamic> json) {
    return FeedingEvent(
      id: json['id'] ?? '',
      deviceId: json['device_id'] ?? '',
      amount: (json['amount'] ?? 0).toDouble(),
      actualAmount: json['actual_amount']?.toDouble(),
      status: _parseStatus(json['status']),
      type: json['type'] ?? 'scheduled',
      errorMessage: json['error_message'],
      scheduledAt: DateTime.parse(json['scheduled_at'] ?? DateTime.now().toIso8601String()),
      completedAt: json['completed_at'] != null ? DateTime.parse(json['completed_at']) : null,
      waterTemperature: json['water_temperature']?.toDouble(),
      dissolvedOxygen: json['dissolved_oxygen']?.toDouble(),
    );
  }

  static FeedingEventStatus _parseStatus(String? status) {
    switch (status) {
      case 'completed': return FeedingEventStatus.completed;
      case 'failed': return FeedingEventStatus.failed;
      case 'pending': return FeedingEventStatus.pending;
      case 'cancelled': return FeedingEventStatus.cancelled;
      default: return FeedingEventStatus.pending;
    }
  }

  @override
  List<Object?> get props => [id, deviceId, amount, status, type, scheduledAt];
}

class FeedCalculationRequest {
  final String speciesId;
  final int fishCount;
  final double averageWeight;
  final double waterTemperature;
  final double? dissolvedOxygen;
  final double? ph;

  FeedCalculationRequest({
    required this.speciesId,
    required this.fishCount,
    required this.averageWeight,
    required this.waterTemperature,
    this.dissolvedOxygen,
    this.ph,
  });

  Map<String, dynamic> toJson() => {
    'species_id': speciesId,
    'fish_count': fishCount,
    'average_weight': averageWeight,
    'water_temperature': waterTemperature,
    if (dissolvedOxygen != null) 'dissolved_oxygen': dissolvedOxygen,
    if (ph != null) 'ph': ph,
  };
}

class FeedCalculationResult {
  final double recommendedAmount;
  final double biomass;
  final double feedingRate;
  final double q10Factor;
  final double? obmSafetyFactor;
  final String recommendation;
  final int suggestedFeedings;

  FeedCalculationResult({
    required this.recommendedAmount,
    required this.biomass,
    required this.feedingRate,
    required this.q10Factor,
    this.obmSafetyFactor,
    required this.recommendation,
    required this.suggestedFeedings,
  });

  factory FeedCalculationResult.fromJson(Map<String, dynamic> json) {
    return FeedCalculationResult(
      recommendedAmount: (json['recommended_amount'] ?? 0).toDouble(),
      biomass: (json['biomass'] ?? 0).toDouble(),
      feedingRate: (json['feeding_rate'] ?? 0).toDouble(),
      q10Factor: (json['q10_factor'] ?? 1).toDouble(),
      obmSafetyFactor: json['obm_safety_factor']?.toDouble(),
      recommendation: json['recommendation'] ?? '',
      suggestedFeedings: json['suggested_feedings'] ?? 3,
    );
  }
}

class FishSpecies {
  final String id;
  final String name;
  final double q10Coefficient;
  final double referenceTemperature;
  final double optimalTempMin;
  final double optimalTempMax;
  final double fingerlingFeedRate;
  final double juvenileFeedRate;
  final double adultFeedRate;

  FishSpecies({
    required this.id,
    required this.name,
    required this.q10Coefficient,
    required this.referenceTemperature,
    required this.optimalTempMin,
    required this.optimalTempMax,
    required this.fingerlingFeedRate,
    required this.juvenileFeedRate,
    required this.adultFeedRate,
  });

  factory FishSpecies.fromJson(Map<String, dynamic> json) {
    return FishSpecies(
      id: json['id'] ?? '',
      name: json['name'] ?? '',
      q10Coefficient: (json['q10_coefficient'] ?? 2.0).toDouble(),
      referenceTemperature: (json['reference_temperature'] ?? 25).toDouble(),
      optimalTempMin: (json['optimal_temp_min'] ?? 24).toDouble(),
      optimalTempMax: (json['optimal_temp_max'] ?? 30).toDouble(),
      fingerlingFeedRate: (json['fingerling_feed_rate'] ?? 8).toDouble(),
      juvenileFeedRate: (json['juvenile_feed_rate'] ?? 4).toDouble(),
      adultFeedRate: (json['adult_feed_rate'] ?? 1.5).toDouble(),
    );
  }
}
