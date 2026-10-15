package com.jetfire_oldsmobile.jetwatchapp.ui.theme

import android.os.Build
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable

private val DarkColorScheme = darkColorScheme(
    primary = Red80,
    secondary = Amber80,
    tertiary = Yellow80,
    background = WarmBgDark,
    surface = WarmBgDark,
)

private val LightColorScheme = lightColorScheme(
    primary = Red40,
    secondary = Amber40,
    tertiary = Yellow40,
    background = WarmBgLight,
    surface = WarmBgLight,
)

@Composable
fun JetWatchAppTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    // Динамический цвет отключён, чтобы бренд не перебивался системными обоями.
    dynamicColor: Boolean = false,
    content: @Composable () -> Unit
) {
    val colorScheme = when {
        dynamicColor && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S -> {
            val context = androidx.compose.ui.platform.LocalContext.current
            if (darkTheme) androidx.compose.material3.dynamicDarkColorScheme(context)
            else androidx.compose.material3.dynamicLightColorScheme(context)
        }
        darkTheme -> DarkColorScheme
        else -> LightColorScheme
    }

    MaterialTheme(
        colorScheme = colorScheme,
        typography = Typography,
        content = content
    )
}