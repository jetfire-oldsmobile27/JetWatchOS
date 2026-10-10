package com.jetfire_oldsmobile.jetwatchapp.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.jetfire_oldsmobile.jetwatchapp.log.AppLog
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext

/** Fullscreen viewer of service.log with search; follows the tail unless searching. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun LogScreen(onBack: () -> Unit) {
    val ver by AppLog.version.collectAsStateWithLifecycle()
    var lines by remember { mutableStateOf(emptyList<String>()) }
    var query by remember { mutableStateOf("") }
    LaunchedEffect(ver) {
        delay(300) // debounce bursts of writes
        lines = withContext(Dispatchers.IO) { AppLog.readTail(5000) }
    }
    val shown = remember(lines, query) { if (query.isBlank()) lines else lines.filter { it.contains(query, ignoreCase = true) } }
    val list = rememberLazyListState()
    LaunchedEffect(shown.size, query) {
        if (query.isBlank() && shown.isNotEmpty()) list.scrollToItem(shown.lastIndex)
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("service.log") },
                navigationIcon = { IconButton(onBack) { Icon(Icons.AutoMirrored.Filled.ArrowBack, "Back") } },
                actions = { IconButton({ AppLog.clear() }) { Icon(Icons.Default.Delete, "Clear") } },
            )
        }
    ) { pad ->
        Column(Modifier.padding(pad).fillMaxSize()) {
            OutlinedTextField(
                query, { query = it }, singleLine = true,
                leadingIcon = { Icon(Icons.Default.Search, null) },
                trailingIcon = { if (query.isNotEmpty()) IconButton({ query = "" }) { Icon(Icons.Default.Close, "Clear search") } },
                placeholder = { Text("Search (${shown.size} lines)") },
                modifier = Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp),
            )
            LazyColumn(Modifier.fillMaxSize().padding(horizontal = 8.dp), state = list) {
                itemsIndexed(shown) { _, line -> Text(highlight(line, query), fontFamily = FontFamily.Monospace, fontSize = 11.sp, lineHeight = 14.sp) }
            }
        }
    }
}

@Composable
private fun highlight(line: String, q: String) = buildAnnotatedString {
    val base = when {
        line.length > 13 && line[13] == 'E' -> Color(0xFFE53935)
        line.length > 13 && line[13] == 'W' -> Color(0xFFFB8C00)
        else -> MaterialTheme.colorScheme.onSurface
    }
    withStyle(SpanStyle(color = base)) {
        if (q.isBlank()) append(line) else {
            var i = 0
            while (i < line.length) {
                val j = line.indexOf(q, i, ignoreCase = true)
                if (j < 0) { append(line.substring(i)); break }
                append(line.substring(i, j))
                withStyle(SpanStyle(background = Color(0xFFFFEB3B), color = Color.Black)) { append(line.substring(j, j + q.length)) }
                i = j + q.length
            }
        }
    }
}
