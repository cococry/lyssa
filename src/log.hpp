#pragma once 

#define LOG_TRACE(...) { printf("Lyssa: [TRACE]: "); printf(__VA_ARGS__); printf("\n"); } 
#define LOG_INFO(...) { printf("Lyssa: [INFO]: "); printf(__VA_ARGS__); printf("\n"); } 
#define LOG_WARN(...) { printf("Lyssa: [WARN]: "); printf(__VA_ARGS__); printf("\n"); } 
#define LOG_ERROR(...) { printf("Lyssa: [ERROR]: "); printf(__VA_ARGS__); printf("\n"); } 
