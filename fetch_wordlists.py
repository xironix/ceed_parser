#!/usr/bin/env python3
"""
Script to download BIP-39 and Monero wordlists for all supported languages
"""

import os
import re
import requests
from pathlib import Path

# Create data directory if it doesn't exist
data_dir = Path("data")
data_dir.mkdir(exist_ok=True)

# Base URLs
TREZOR_BASE = "https://raw.githubusercontent.com/trezor/trezor-firmware/master/crypto/bip39/wordlist"
MONERO_BASE = "https://raw.githubusercontent.com/monero-project/monero/master/src/mnemonics"

def download_file(url, output_path):
    """Download a file from URL to the specified path"""
    response = requests.get(url)
    if response.status_code == 200:
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(response.text)
        return True
    return False

def extract_monero_words(content):
    """Extract words from Monero header files"""
    # Match words in the format "word",
    pattern = re.compile(r'^\s+"([a-z]+)",', re.MULTILINE)
    matches = pattern.findall(content)
    return matches

# BIP-39 Wordlists
print("⬇️ Downloading BIP-39 wordlists...")
languages = [
    "english", "spanish", "french", "italian", "portuguese", 
    "czech", "japanese", "korean", "chinese_simplified", "chinese_traditional"
]

for lang in languages:
    print(f"  🔄 Downloading BIP-39 {lang} wordlist...")
    url = f"{TREZOR_BASE}/{lang}.txt"
    output_path = data_dir / f"{lang}.txt"
    
    if download_file(url, output_path):
        word_count = sum(1 for _ in open(output_path, 'r', encoding='utf-8'))
        print(f"  ✅ BIP-39 {lang}: {word_count} words")
    else:
        print(f"  ❌ Failed to download BIP-39 {lang} wordlist")

# Monero Wordlists
print("\n⬇️ Downloading Monero wordlists...")
monero_languages = [
    "english", "spanish", "portuguese", "french", "italian", 
    "german", "russian", "japanese", "chinese_simplified", 
    "dutch", "esperanto", "lojban"
]

for lang in monero_languages:
    print(f"  🔄 Downloading Monero {lang} wordlist...")
    url = f"{MONERO_BASE}/{lang}.h"
    response = requests.get(url)
    
    if response.status_code == 200:
        words = extract_monero_words(response.text)
        output_path = data_dir / f"monero_{lang}.txt"
        
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(words))
            
        print(f"  ✅ Monero {lang}: {len(words)} words")
    else:
        print(f"  ❌ Failed to download Monero {lang} wordlist")

# Verify the files
print("\n🔍 Verifying all wordlists...")
total_files = len(list(data_dir.glob("*.txt")))
print(f"📊 Total wordlist files: {total_files}")

# Check specific important files
for lang in ["english", "spanish", "french"]:
    bip39_path = data_dir / f"{lang}.txt"
    monero_path = data_dir / f"monero_{lang}.txt"
    
    if bip39_path.exists() and bip39_path.stat().st_size > 0:
        print(f"✅ BIP-39 {lang} wordlist exists and is not empty")
    else:
        print(f"❌ ERROR: BIP-39 {lang} wordlist is missing or empty!")
    
    if monero_path.exists() and monero_path.stat().st_size > 0:
        print(f"✅ Monero {lang} wordlist exists and is not empty")
    else:
        print(f"❌ ERROR: Monero {lang} wordlist is missing or empty!")

print("\n✨ All wordlists downloaded successfully! ✨")
print("🧠 Ready for mnemonic processing and benchmarking.") 