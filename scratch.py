import os
import re

files_hpp = [
    "src/http/HttpParser.hpp",
    "src/storage/SqliteStore.hpp",
    "src/proxy/CertGenerator.hpp",
    "src/proxy/MitmSession.hpp",
    "src/proxy/ProxyServer.hpp",
    "src/proxy/Session.hpp",
]

for f in files_hpp:
    with open(f, 'r') as file:
        content = file.read()
    if '#include <memory>' not in content:
        content = re.sub(r'(#pragma once\n)', r'\1#include <memory>\n', content, count=1)
    content = re.sub(r'Impl\* impl_ = nullptr;', r'std::unique_ptr<Impl> impl_;', content)
    content = re.sub(r'Impl\* impl_;', r'std::unique_ptr<Impl> impl_;', content)
    with open(f, 'w') as file:
        file.write(content)

files_cpp = [
    "src/http/HttpParser.cpp",
    "src/storage/SqliteStore.cpp",
    "src/proxy/CertGenerator.cpp",
    "src/proxy/MitmSession.cpp",
    "src/proxy/ProxyServer.cpp",
    "src/proxy/Session.cpp",
]

for f in files_cpp:
    with open(f, 'r') as file:
        content = file.read()
    # Replace new Impl(...) with std::make_unique<Impl>(...)
    content = re.sub(r'impl_\(new Impl\s*\{?(.*?)\}?\)', r'impl_(std::make_unique<Impl>(\1))', content)
    content = re.sub(r'impl_ = new Impl\s*\{?(.*?)\}?;', r'impl_ = std::make_unique<Impl>(\1);', content)
    # Remove delete impl_
    content = re.sub(r'delete impl_;\s*', '', content)
    with open(f, 'w') as file:
        file.write(content)

print("Done replacing pimpl pointers.")
