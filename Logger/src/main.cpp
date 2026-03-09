//
// Created by lacas on 2026/3/8.
//

#include "iostream"
#include "LoggerClass.h"
using namespace std;

int main(){
    Logger& logger = Logger::get_instance();
    string str = "hello world";
    logger.Info("{}",str);
    return 0;
}