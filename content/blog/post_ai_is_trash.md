---
title: "AI is Trash Unless Locally"
date: "2025-06-07"
category: "Tech"
---

# AI is Trash Unless Locally

_Jun 07, 2025_

Running AI models locally doesn't have to be complicated. With [Ollama](https://ollama.com/), it's surprisingly simple and powerful.

Ollama is a lightweight tool that lets you run open source large language models directly on your machine. Whether you're experimenting, building apps, or exploring AI, it supports models like DeepSeek R1, Qwen 3, Llama 3.3, Qwen 2.5 VL, Gemma 3, and many others.

## Why Run Models Locally?

Local deployment gives you privacy, control, and offline access—Ollama makes this easier than ever.

## Installation Guide

### Windows

1. Download the installer from [ollama.com/download](https://ollama.com/download)
2. Run the `.exe` file and follow the prompts.
3. Open Command Prompt or PowerShell and run:

```bash
ollama run llama3
```

### macOS

1. Open Terminal and run:

```bash
curl -fsSL https://ollama.com/install.sh | sh
```

2. Then start a model:

```bash
ollama run llama3
```

### Linux

1. Install using the command:

```bash
curl -fsSL https://ollama.com/install.sh | sh
```

2. Run a model:

```bash
ollama run llama3
```

### Docker

1. Make sure Docker is installed and running.
2. Pull and run the Ollama image:

```bash
docker run -d -p 11434:11434 ollama/ollama
```

3. Then, from your host machine, you can run:

```bash
ollama run llama3
```

## Final Thoughts

Ollama is a beautiful tool for anyone looking to explore or integrate local AI capabilities. It's free, open, and supports a growing list of models. Give it a try—you might be surprised how easy and fast it is to get started.
