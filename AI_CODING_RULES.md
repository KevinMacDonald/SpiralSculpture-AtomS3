# AI Coding Assistant Rules

This file contains a set of rules for the AI coding assistant to follow when working on this project.

## General Rules

1.  **Gemini_TODO**: If you see a comment in the code that starts with `Gemini_TODO`, please read the comment, act on the instruction, and then remove the `Gemini_TODO` item from the file.

2.  **Concurrency**: Whenever you are contemplating multi-threading concerns that involve mutexes, semaphores, or other complex concurrency primitives, please discuss the approach with me first before proposing significant coding changes. We need to be extra careful about stability.