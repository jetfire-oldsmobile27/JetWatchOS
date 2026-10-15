package com.jetfire_oldsmobile.jetwatchapp.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.jetfire_oldsmobile.jetwatchapp.R
import com.jetfire_oldsmobile.jetwatchapp.ui.WatchViewModel
import java.time.LocalDate
import java.time.format.DateTimeFormatter
import java.util.Locale

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun StatsScreen(vm: WatchViewModel, onBack: () -> Unit) {
    val st by vm.state.collectAsStateWithLifecycle()
    val cfg by vm.settings.collectAsStateWithLifecycle()
    val fmt = DateTimeFormatter.ofPattern("EEE, dd MMM", Locale.getDefault())
    val goal = cfg.goal.coerceAtLeast(1)
    val sorted = st.week.sortedByDescending { it.ymd }
    val max = maxOf(goal, sorted.maxOfOrNull { it.steps } ?: goal).toFloat()

    Scaffold(topBar = {
        TopAppBar(
            title = { Text(stringResource(R.string.stats_title)) },
            navigationIcon = {
                IconButton(onBack) { Icon(Icons.AutoMirrored.Filled.ArrowBack, null) }
            },
        )
    }) { pad ->
        LazyColumn(
            Modifier.padding(pad).fillMaxSize().padding(horizontal = 16.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            item { Spacer(Modifier.height(8.dp)) }
            items(sorted, key = { it.ymd }) { d ->
                val reached = d.steps >= goal
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.width(120.dp)) {
                        Text(
                            runCatching { LocalDate.parse(d.ymd).format(fmt) }.getOrDefault(d.ymd),
                            style = MaterialTheme.typography.bodyMedium,
                        )
                        Text("${d.steps} / $goal", style = MaterialTheme.typography.labelSmall)
                    }
                    Spacer(Modifier.width(8.dp))
                    Box(
                        Modifier.weight(1f).height(18.dp)
                            .background(MaterialTheme.colorScheme.surfaceVariant, RoundedCornerShape(9.dp))
                    ) {
                        Box(
                            Modifier.fillMaxHeight()
                                .fillMaxWidth((d.steps / max).coerceIn(0.02f, 1f))
                                .background(
                                    if (reached) MaterialTheme.colorScheme.tertiary
                                    else MaterialTheme.colorScheme.primary,
                                    RoundedCornerShape(9.dp),
                                )
                        )
                    }
                }
            }
            item { Spacer(Modifier.height(16.dp)) }
        }
    }
}