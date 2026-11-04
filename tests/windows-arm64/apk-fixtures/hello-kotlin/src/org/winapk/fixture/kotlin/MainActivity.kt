package org.winapk.fixture.kotlin

import android.app.Activity
import android.os.Bundle
import android.util.Log
import android.widget.TextView

/** A word and how often it occurs. */
data class Count(val word: String, val n: Int)

/** Counts words with the Kotlin standard library: lazy, Regex, grouping, comparators. */
object Words {
    val sample by lazy { "the quick brown fox jumps over the lazy dog the end" }

    /** Returns the words of [text], most frequent first, then alphabetically. */
    fun counts(text: String): List<Count> =
        Regex("""\s+""").split(text).groupingBy { it }.eachCount()
            .map { (w, n) -> Count(w, n) }
            .sortedWith(compareByDescending<Count> { it.n }.thenBy { it.word })
}

/** Returns this number times itself. */
fun Int.squared() = this * this

/**
 * Shows the T04 result: values computed with the Kotlin standard library.
 *
 * Notes: logs "T04: result=…" (scripts/windows/test.ps1 asserts it, PLAN.md §8 G1.6) and the
 * lifecycle. Passing proves this fixture's Kotlin runs on the DEX path, not that every Kotlin
 * or Compose app does.
 */
class MainActivity : Activity() {
    override fun onCreate(saved: Bundle?) {
        super.onCreate(saved)
        Log.i(TAG, "lifecycle=onCreate")
        val top = Words.counts(Words.sample).take(3).joinToString(",") { "${it.word}:${it.n}" }
        val squares = generateSequence(1) { it + 1 }.map { it.squared() }.takeWhile { it < 50 }.sum()
        val copy = Count("fox", 1).copy(n = 2)
        val result = buildString {
            append("top=").append(top)
            append(" squares=").append(squares)
            append(" copy=").append(copy)
        }
        Log.i(TAG, "result=$result")
        setContentView(TextView(this).apply { text = result })
    }

    override fun onResume() {
        super.onResume()
        Log.i(TAG, "lifecycle=onResume")
    }

    private companion object {
        const val TAG = "T04"
    }
}
