# Clouds 颗粒引擎移植验证

本次实现采用 [Mutable Instruments 的官方 Clouds 源码](https://github.com/pichenettes/eurorack/tree/08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4/clouds)，范围是正常的 Granular 模式。控制含义对照[官方手册](https://pichenettes.github.io/mutable-instruments-documentation/modules/clouds/manual/)。具体来源版本、MIT 许可、移植调整见 [DSP 目录说明](../Source/DSP/Clouds/README.md)。

## 声音与实时处理

- 保留原版 32 kHz、32 帧处理块、立体声 16-bit 录音、32 个并发颗粒，以及调度、窗口、扩散、反馈和混响算法。
- 宿主两端使用抗混叠 SRC，支持 8–384 kHz；内部固定时间轴不随宿主块大小变化。预分配内存，共享不可变 SRC 系数，音频处理与 reset 不取锁、不分配。
- 修复原代码依赖全局零初始化、全局 RNG、解冻 tail 越界及查表终点越界的问题。
- 冷启动录音前 160 个内部采样加入渐入，避免已有 grain 在窗口中途读到非零录音起点。已有录音中的后续音符不另加攻击包络。
- 部分录音可以立即 Freeze。首次有效录音之前请求 Freeze 时，先采集有信号的完整录音窗口。录音内容不随项目、预设或 A/B 保存。
- Fire 保留线性的 Mix 和透明 dry path；这不是硬件逐位仿真。原版颗粒的变调插值仍保留其声音特征，SRC 的抗混叠不能等同于理想的颗粒变调抗混叠。

48 kHz、0.2 恒定输入、默认 Clouds 参数、100% wet 的冷启动检查中，两声道最大相邻采样变化为 **0.001320 / 0.001387**；reset 后重复测量一致。该探针专门排查录音起点的人工跳变，不代表对所有音色的主观评价。

## 状态及宿主兼容

原 Type、六个 Control、Enabled、Order、旧参数顺序与 AU version hint 保持不变。200 个扩展参数继续保留原索引和 hint 5；新状态使用 `cloudsSchemaVersion=2`。所有 Granular 统一运行 Clouds，旧的 40 个 engine ID 仅是不可自动化的保留字段，写入 0/1 都不改变音频。

测试覆盖预设、host state、双向 A/B、Copy、频段增删迁移、slot 复用、三路新增 LFO 控制和损坏状态的事务拒绝。原有类型切换淡入淡出保留；引擎选择控件与旧颗粒 DSP 已移除。非 Granular 插槽跳过新增调制查询。

Freeze 和满 Feedback 使用无限尾音。当前 CLAP 依赖中的快速整数转换会把正无穷变成零，因此构建时生成修补后的 wrapper，使用有界转换并在音频线程通知 `tail.changed`。原始依赖不修改；上游补丁位置改变时配置失败，要求重新审核。VST3/AU 的真实无穷返回值保持不变。

## 2026-09-16 初次移植验证记录

```sh
cmake --build Builds --target Tests Fire -j6
Builds/Tests '[clouds],[clouds-ui],[control-panel][primary-button]' --reporter compact
```

`CloudsEngineTests` 的独立 AddressSanitizer、UndefinedBehaviorSanitizer 和 float-cast-overflow 检查通过 **10 个测试 / 32181 条断言**，包含极值、短 Freeze、实例隔离、reset 和真实的 SRC 混叠抑制检查。完整插件的测试还覆盖 routing、状态迁移和 UI；原生菜单测试依赖可用的 macOS 显示会话。

两套完整编辑器页面在 1000×500、1400×700、2000×1000 检查边界与重叠，Band/Master 最小尺寸 PNG 已人工查看。可用 `FIRE_CLOUDS_UI_SNAPSHOT=/tmp/clouds.png` 启用测试截图。

初次移植的回归采用全量运行加受影响范围复测，覆盖当时 **800 个用例：798 通过，2 个原生菜单用例因显示环境跳过**。全量初跑的 794 项中，两项旧测试的按钮数量和频段参数清单尚未包含新增控制；保留原检查、补全独立预期后，104 项定向回归通过 490794 条断言（其中同样有 2 项环境跳过）。该轮也包含新增的实际 `FireAudioProcessor::processBlock` 检查，确认 Master/Band 引擎选择、Feedback/Reverb、额外 LFO 及 Freeze 到达音频链。

最终尾音修改另通过 2 项 / 71 条断言：休眠、关闭或被其他频段 Solo 静音的 Clouds 不误报无限尾音；CLAP 对无穷、非法值及极大长度的转换安全。`Tests`、C++17 插件共享代码与修补后的 CLAP wrapper 均编译成功。未以真实硬件或 DAW 录音逐样本比对这些结果。

独立 CPU 探针包含单个颗粒内核、反馈/混响和双向 SRC，不包含 Fire 其他 DSP、外层调制及 UI，不能直接当作 DAW 的 CPU 表读数。每个场景预热 1 秒，再取 5 轮各 2 秒音频处理时间的中位数：

```sh
clang++ -std=c++23 -O3 -fno-fast-math -DNDEBUG -I Source \
  benchmarks/CloudsCpuProbe.cpp Source/DSP/Clouds/CloudsEngine.cpp \
  -o /tmp/fire-clouds-cpu
/tmp/fire-clouds-cpu
```

本机 AppleClang 17 / arm64，关闭其他构建与测试后测得：

| 宿主采样率 | 场景 | 每 128 samples 耗时 | 单核时间占比估算 |
| --- | --- | ---: | ---: |
| 48 kHz | 默认 | 21.48 µs | 0.805% |
| 48 kHz | Density 最大随机 | 30.44 µs | 1.141% |
| 48 kHz | 高密度、长颗粒、最大反馈/混响、+24 st | 30.55 µs | 1.146% |
| 96 kHz | 默认 | 18.73 µs | 1.405% |
| 96 kHz | Density 最大随机 | 23.67 µs | 1.775% |
| 192 kHz | 默认 | 16.83 µs | 2.525% |
| 192 kHz | Density 最大随机 | 19.06 µs | 2.859% |

完整 Clouds 引擎承担额外 SRC、扩散和混响，不能当作旧简化颗粒内核的等成本替换。闲置插槽不运行该引擎。这是初次移植版本的测量；当前已删除旧引擎。以上数值只代表这些输入和参数，不是所有工程的性能保证。

## 2026-09-19：仅保留 Clouds，并更新 JUCE

项目内 JUCE 固定为官方 9.0.2 发布提交 `72782788ce18c2d4d760b28e0921d6ffc6431102`。
CMake 和 Projucer 使用项目内 `JUCE/modules`；包装源文件由同版本 Projucer 重新生成。
Inspector 字体测宽、CLAP 的 headless 参数头文件、轨道颜色和窗口 API 已适配，
Linux CI 补充 JUCE 9 所需的 XInput/EGL 开发依赖。

删除了 Legacy grain 调度、读样代码、专用重采样表和旧引擎测试。
旧状态中只有确实属于 Legacy 的 Granular slot 会做一次参数迁移：Size 按毫秒
映射到 Clouds 的对数尺寸，Density 近似还原规律颗粒速率，Position 按录音窗
及播放头间距映射，Spray 映射到 50–100% Texture。Pitch/Mix 保持原值。
已有 Clouds v1 和已迁移 v2 状态不再转换。这是参数近似迁移，不能保持旧引擎音色。

真正发生迁移时重置原先不生效的 Clouds 扩展值及其调制路由，并清除原引擎的
等响度校准；没有发生迁移的 Clouds 工程继续保留这些状态。
输入文档必须通过原始完整性校验后才能迁移或参与预设等价比较。
保留的 engine 参数不能使声音重启、切换引擎或把预设标记为修改。

本轮在 JUCE 9.0.2 / AppleClang 17 / arm64 / Release 下完成全量验证：
**800 项用例中 798 项通过，2 项原生菜单用例因显示环境跳过，1633762 条断言通过**。
AU、VST3、CLAP 均编译完成并通过包签名校验；最终 VST3 通过本机 pluginval
strictness 5，覆盖 44.1/48/96 kHz、64–1024 样本块、编辑器、状态与自动化。
仅 Clouds 的 Band/Master 最小窗口截图也已检查。

交付目录为 `Builds/JUCE9-Release/Fire_artefacts/Release/`，本次配置
`FIRE_INSTALL_PLUGINS=OFF`，产物保留在项目内。AU 的资源签名在不安装时也会
显式生成；开启自动安装时仍保留原来的复制行为。
