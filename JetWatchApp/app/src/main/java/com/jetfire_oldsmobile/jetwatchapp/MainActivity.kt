package com.jetfire_oldsmobile.jetwatchapp

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.annotation.StringRes
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import com.jetfire_oldsmobile.jetwatchapp.ui.WatchViewModel
import com.jetfire_oldsmobile.jetwatchapp.ui.screens.AppsScreen
import com.jetfire_oldsmobile.jetwatchapp.ui.screens.ControlScreen
import com.jetfire_oldsmobile.jetwatchapp.ui.screens.HomeScreen
import com.jetfire_oldsmobile.jetwatchapp.ui.screens.LogScreen
import com.jetfire_oldsmobile.jetwatchapp.ui.screens.StatsScreen
import com.jetfire_oldsmobile.jetwatchapp.ui.theme.JetWatchAppTheme

private data class Tab(val route: String, @StringRes val labelRes: Int, val icon: ImageVector)

private val tabs = listOf(
    Tab("home", R.string.tab_home, Icons.Default.Home),
    Tab("control", R.string.tab_control, Icons.Default.Settings),
    Tab("apps", R.string.tab_apps, Icons.Default.Notifications),
)

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            JetWatchAppTheme {
                val nav = rememberNavController()
                val vm: WatchViewModel = viewModel()
                val snack = remember { SnackbarHostState() }
                LaunchedEffect(Unit) { vm.msg.collect { snack.showSnackbar(it) } }
                val route = nav.currentBackStackEntryAsState().value?.destination?.route

                Scaffold(
                    modifier = Modifier.fillMaxSize(),
                    snackbarHost = { SnackbarHost(snack) },
                    bottomBar = {
                        if (route != "logs" && route != "stats") NavigationBar {
                            tabs.forEach { t ->
                                NavigationBarItem(
                                    selected = route == t.route,
                                    onClick = {
                                        nav.navigate(t.route) {
                                            popUpTo("home"); launchSingleTop = true
                                        }
                                    },
                                    icon = { Icon(t.icon, stringResource(t.labelRes)) },
                                    label = { Text(stringResource(t.labelRes)) },
                                )
                            }
                        }
                    },
                ) { pad ->
                    NavHost(nav, startDestination = "home", modifier = Modifier.padding(pad)) {
                        composable("home") {
                            HomeScreen(
                                vm,
                                onOpenLogs = { nav.navigate("logs") },
                                onOpenStats = { nav.navigate("stats") },
                            )
                        }
                        composable("control") { ControlScreen(vm) }
                        composable("apps") { AppsScreen(vm) }
                        composable("logs") { LogScreen(onBack = { nav.popBackStack() }) }
                        composable("stats") { StatsScreen(vm, onBack = { nav.popBackStack() }) }
                    }
                }
            }
        }
    }
}