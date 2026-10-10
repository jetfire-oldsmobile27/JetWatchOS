package com.jetfire_oldsmobile.jetwatchapp.health

import android.content.Context
import androidx.health.connect.client.HealthConnectClient
import androidx.health.connect.client.permission.HealthPermission
import androidx.health.connect.client.records.ExerciseSessionRecord
import androidx.health.connect.client.records.StepsRecord
import androidx.health.connect.client.records.metadata.Device
import androidx.health.connect.client.records.metadata.Metadata
import androidx.health.connect.client.request.AggregateRequest
import androidx.health.connect.client.time.TimeRangeFilter
import com.jetfire_oldsmobile.jetwatchapp.proto.Training
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId

enum class HcStatus { Unavailable, NeedsUpdate, NoPermission, Ready }

/**
 * Same principles as the Pedometer app: one client, a fixed permission set,
 * and idempotent writes (clientRecordId + increasing clientRecordVersion => upsert, no duplicates).
 * NOTE: reading in background needs READ_HEALTH_DATA_IN_BACKGROUND, so reads happen from UI only;
 * writes are allowed from the foreground service.
 */
class HealthConnectManager(private val ctx: Context) {
    val permissions = setOf(
        HealthPermission.getReadPermission(StepsRecord::class),
        HealthPermission.getWritePermission(StepsRecord::class),
        HealthPermission.getReadPermission(ExerciseSessionRecord::class),
        HealthPermission.getWritePermission(ExerciseSessionRecord::class),
    )

    private val client: HealthConnectClient by lazy { HealthConnectClient.getOrCreate(ctx) }

    suspend fun status(): HcStatus = when (HealthConnectClient.getSdkStatus(ctx)) {
        HealthConnectClient.SDK_UNAVAILABLE -> HcStatus.Unavailable
        HealthConnectClient.SDK_UNAVAILABLE_PROVIDER_UPDATE_REQUIRED -> HcStatus.NeedsUpdate
        else -> if (client.permissionController.getGrantedPermissions().containsAll(permissions)) HcStatus.Ready
        else HcStatus.NoPermission
    }

    /** Total steps for today from ALL sources (Health Connect de-duplicates by data-origin priority). */
    suspend fun readTodaySteps(): Long {
        val zone = ZoneId.systemDefault()
        val start = LocalDate.now(zone).atStartOfDay(zone).toInstant()
        val res = client.aggregate(
            AggregateRequest(setOf(StepsRecord.COUNT_TOTAL), TimeRangeFilter.between(start, Instant.now()))
        )
        return res[StepsRecord.COUNT_TOTAL] ?: 0L
    }

    /** One record per day, upserted on every sync. ymd = yyyy-MM-dd (watch local date). */
    suspend fun writeDailySteps(ymd: String, steps: Long) {
        if (steps <= 0) return
        val zone = ZoneId.systemDefault()
        val date = LocalDate.parse(ymd)
        val start = date.atStartOfDay(zone).toInstant()
        val endOfDay = date.plusDays(1).atStartOfDay(zone).toInstant()
        val now = Instant.now()
        val end = if (endOfDay.isBefore(now)) endOfDay else now
        if (!end.isAfter(start)) return
        val rec = StepsRecord(
            count = steps,
            startTime = start,
            startZoneOffset = zone.rules.getOffset(start),
            endTime = end,
            endZoneOffset = zone.rules.getOffset(end),
            metadata = Metadata.autoRecorded(
                device = Device(type = Device.TYPE_WATCH),
                clientRecordId = "jetwatch-steps-$ymd",
                clientRecordVersion = System.currentTimeMillis(),
            ),
        )
        client.insertRecords(listOf(rec))
    }

    suspend fun writeTraining(t: Training) {
        val zone = ZoneId.systemDefault()
        val start = Instant.ofEpochSecond(t.start)
        val end = start.plusSeconds(t.dur.coerceAtLeast(1))
        val rec = ExerciseSessionRecord(
            startTime = start,
            startZoneOffset = zone.rules.getOffset(start),
            endTime = end,
            endZoneOffset = zone.rules.getOffset(end),
            exerciseType = when (t.type) {
                "walk" -> ExerciseSessionRecord.EXERCISE_TYPE_WALKING
                "run" -> ExerciseSessionRecord.EXERCISE_TYPE_RUNNING
                "bike" -> ExerciseSessionRecord.EXERCISE_TYPE_BIKING
                "hike" -> ExerciseSessionRecord.EXERCISE_TYPE_HIKING
                else -> ExerciseSessionRecord.EXERCISE_TYPE_OTHER_WORKOUT
            },
            title = "Watch ${t.type}",
            metadata = Metadata.autoRecorded(
                device = Device(type = Device.TYPE_WATCH),
                clientRecordId = "jetwatch-train-${t.id}-${t.start}",
                clientRecordVersion = 1,
            ),
        )
        client.insertRecords(listOf(rec))
    }
}
