// Detail/benchmark UI translations.
// Entry format: { L"中文", L"English", L"Français", L"Deutsch", L"Español", L"日本語", L"한국어", L"Русский" },
//
// Keys are the exact Chinese source strings passed to Tr() by
// DetailPane.cpp / BenchmarkPane.cpp / ThroughputChart.cpp.  Empty cells fall
// back to English.

// ---------------------------------------------------------------------------
// Detail pane: header, tabs, list columns
// ---------------------------------------------------------------------------
{ L"选择一个设备", L"Select a device", L"Sélectionnez un appareil", L"Gerät auswählen", L"Seleccione un dispositivo", L"デバイスを選択", L"장치 선택", L"Выберите устройство" },
{ L"左侧树中显示 USB 级联关系", L"The left tree shows the USB topology", L"L'arborescence de gauche affiche la topologie USB", L"Die linke Struktur zeigt die USB-Topologie", L"El árbol izquierdo muestra la topología USB", L"左側のツリーに USB トポロジを表示します", L"왼쪽 트리에 USB 토폴로지를 표시합니다", L"В левом дереве показана топология USB" },
{ L"属性", L"Property", L"Propriété", L"Eigenschaft", L"Propiedad", L"プロパティ", L"속성", L"Свойство" },
{ L"值", L"Value", L"Valeur", L"Wert", L"Valor", L"値", L"값", L"Значение" },
{ L"当前", L"Current", L"Actuel", L"Aktuell", L"Actual", L"現在値", L"현재", L"Текущее" },
{ L"最差", L"Worst", L"Pire", L"Schlechtester Wert", L"Peor", L"最悪値", L"최악", L"Худшее" },
{ L"阈值", L"Threshold", L"Seuil", L"Schwellenwert", L"Umbral", L"しきい値", L"임계값", L"Порог" },
{ L"原始值", L"Raw value", L"Valeur brute", L"Rohwert", L"Valor sin procesar", L"生の値", L"원시 값", L"Исходное значение" },
{ L"状态", L"Status", L"État", L"Status", L"Estado", L"状態", L"상태", L"Состояние" },
{ L"设备信息", L"Device info", L"Informations sur l'appareil", L"Geräteinformationen", L"Información del dispositivo", L"デバイス情報", L"장치 정보", L"Сведения об устройстве" },
{ L"SMART / 健康", L"SMART / Health", L"SMART / Santé", L"SMART / Zustand", L"SMART / Salud", L"SMART / 健康", L"SMART / 상태", L"SMART / Здоровье" },
{ L"测速", L"Speed test", L"Test de vitesse", L"Geschwindigkeitstest", L"Prueba de velocidad", L"速度テスト", L"속도 테스트", L"Тест скорости" },
{ L"正在扫描 USB 设备…", L"Scanning USB devices…", L"Analyse des appareils USB…", L"USB-Geräte werden gesucht…", L"Buscando dispositivos USB…", L"USB デバイスをスキャンしています…", L"USB 장치 검색 중…", L"Сканирование устройств USB…" },
{ L"正在读取 HUB 端口、描述符、存储与 SMART 信息", L"Reading HUB ports, descriptors, storage and SMART data", L"Lecture des ports HUB, des descripteurs, du stockage et des données SMART", L"HUB-Anschlüsse, Deskriptoren, Speicher und SMART-Daten werden gelesen", L"Leyendo puertos HUB, descriptores, almacenamiento y datos SMART", L"HUB ポート、記述子、ストレージ、SMART 情報を読み取っています", L"HUB 포트, 설명자, 저장소 및 SMART 정보를 읽는 중", L"Чтение портов HUB, дескрипторов, накопителей и данных SMART" },
{ L"未选择设备", L"No device selected", L"Aucun appareil sélectionné", L"Kein Gerät ausgewählt", L"Ningún dispositivo seleccionado", L"デバイスが選択されていません", L"선택된 장치 없음", L"Устройство не выбрано" },
{ L"在左侧树中选择一个节点", L"Select a node in the left tree", L"Sélectionnez un nœud dans l'arborescence de gauche", L"Wählen Sie einen Knoten in der linken Struktur", L"Seleccione un nodo en el árbol izquierdo", L"左側のツリーでノードを選択してください", L"왼쪽 트리에서 노드를 선택하세요", L"Выберите узел в левом дереве" },
{ L"另有 %d 条提示，见导出报告。", L"%d more tips, see the exported report.", L"%d autres conseils, voir le rapport exporté.", L"%d weitere Hinweise, siehe exportierten Bericht.", L"%d avisos más, consulte el informe exportado.", L"他に %d 件のヒントがあります（エクスポートしたレポートを参照）。", L"추가 팁 %d개, 내보낸 보고서를 참조하세요.", L"Ещё советов: %d, см. экспортированный отчёт." },

// ---------------------------------------------------------------------------
// Detail pane: SMART page
// ---------------------------------------------------------------------------
{ L"仅 USB 存储设备提供 SMART / 健康数据", L"SMART / health data is only available for USB storage devices", L"Les données SMART / de santé ne sont disponibles que pour les périphériques de stockage USB", L"SMART-/Zustandsdaten sind nur für USB-Speichergeräte verfügbar", L"Los datos SMART / de salud solo están disponibles en dispositivos de almacenamiento USB", L"SMART / 健康データは USB ストレージデバイスでのみ利用できます", L"SMART / 상태 데이터는 USB 저장 장치에서만 제공됩니다", L"Данные SMART / состояния доступны только для USB-накопителей" },
{ L"说明", L"Note", L"Remarque", L"Hinweis", L"Nota", L"説明", L"설명", L"Примечание" },
{ L"信息", L"Info", L"Info", L"Info", L"Info", L"情報", L"정보", L"Информация" },
{ L"没有可显示的 SMART 属性", L"No SMART attributes to display", L"Aucun attribut SMART à afficher", L"Keine SMART-Attribute anzuzeigen", L"No hay atributos SMART para mostrar", L"表示できる SMART 属性がありません", L"표시할 SMART 속성이 없습니다", L"Нет атрибутов SMART для отображения" },

// ---------------------------------------------------------------------------
// Detail pane: context menu + driver-row detection
// ---------------------------------------------------------------------------
{ L"复制值", L"Copy value", L"Copier la valeur", L"Wert kopieren", L"Copiar valor", L"値をコピー", L"값 복사", L"Копировать значение" },
{ L"复制整行", L"Copy row", L"Copier la ligne", L"Zeile kopieren", L"Copiar fila", L"行をコピー", L"행 복사", L"Копировать строку" },
{ L"复制全部", L"Copy all", L"Tout copier", L"Alles kopieren", L"Copiar todo", L"すべてコピー", L"모두 복사", L"Копировать всё" },
{ L"定位到驱动文件", L"Locate driver file", L"Localiser le fichier du pilote", L"Treiberdatei anzeigen", L"Localizar el archivo del controlador", L"ドライバーファイルを表示", L"드라이버 파일 찾기", L"Показать файл драйвера" },
{ L"打开 INF 所在目录", L"Open INF folder", L"Ouvrir le dossier INF", L"INF-Ordner öffnen", L"Abrir la carpeta del INF", L"INF のフォルダーを開く", L"INF 폴더 열기", L"Открыть папку INF" },
{ L"驱动", L"Driver", L"Pilote", L"Treiber", L"Controlador", L"ドライバー", L"드라이버", L"Драйвер" },
{ L"服务", L"Service", L"Service", L"Dienst", L"Servicio", L"サービス", L"서비스", L"Служба" },

// ---------------------------------------------------------------------------
// Benchmark page: captions, buttons, warnings
// ---------------------------------------------------------------------------
{ L"选择一个 USB 存储设备以进行测速。", L"Select a USB storage device to run the speed test.", L"Sélectionnez un périphérique de stockage USB pour lancer le test de vitesse.", L"Wählen Sie ein USB-Speichergerät für den Geschwindigkeitstest.", L"Seleccione un dispositivo de almacenamiento USB para realizar la prueba de velocidad.", L"速度テストを実行する USB ストレージデバイスを選択してください。", L"속도 테스트를 실행할 USB 저장 장치를 선택하세요.", L"Выберите USB-накопитель для теста скорости." },
{ L"测试卷:", L"Test volume:", L"Volume de test :", L"Testvolume:", L"Volumen de prueba:", L"テストボリューム:", L"테스트 볼륨:", L"Тестовый том:" },
{ L"大文件大小:", L"Large file size:", L"Taille du gros fichier :", L"Größe der großen Datei:", L"Tamaño del archivo grande:", L"大ファイルサイズ:", L"대용량 파일 크기:", L"Размер большого файла:" },
{ L"开始测速", L"Start speed test", L"Démarrer le test", L"Test starten", L"Iniciar prueba", L"速度テスト開始", L"속도 테스트 시작", L"Начать тест" },
{ L"取消", L"Cancel", L"Annuler", L"Abbrechen", L"Cancelar", L"キャンセル", L"취소", L"Отмена" },
{ L"等待测速。", L"Waiting for the speed test.", L"En attente du test de vitesse.", L"Warten auf Geschwindigkeitstest.", L"En espera de la prueba de velocidad.", L"速度テストを待機しています。", L"속도 테스트 대기 중입니다.", L"Ожидание теста скорости." },
{ L"测速会在所选卷上创建临时文件，仅通过文件系统写入，不会直接写裸盘；测试完成后自动删除。测试会产生写入、占用空间并使设备发热，请确认卷上有足够空间。", L"The speed test creates a temporary file on the selected volume, writes only through the file system and never directly to the raw disk; the file is deleted automatically when the test finishes. The test performs real writes, consumes space and heats the device; make sure the volume has enough free space.", L"Le test de vitesse crée un fichier temporaire sur le volume sélectionné, écrit uniquement via le système de fichiers et jamais directement sur le disque brut ; le fichier est supprimé automatiquement à la fin du test. Le test effectue de vraies écritures, occupe de l'espace et fait chauffer l'appareil ; vérifiez que le volume dispose de suffisamment d'espace.", L"Der Geschwindigkeitstest legt eine temporäre Datei auf dem ausgewählten Volume an, schreibt nur über das Dateisystem und nie direkt auf den Rohdatenträger; die Datei wird nach dem Test automatisch gelöscht. Der Test verursacht echte Schreibvorgänge, belegt Speicherplatz und erwärmt das Gerät; stellen Sie sicher, dass auf dem Volume genügend Speicherplatz frei ist.", L"La prueba de velocidad crea un archivo temporal en el volumen seleccionado, escribe solo a través del sistema de archivos y nunca directamente en el disco sin formato; el archivo se elimina automáticamente al finalizar. La prueba realiza escrituras reales, ocupa espacio y calienta el dispositivo; asegúrese de que el volumen tenga suficiente espacio libre.", L"速度テストは選択したボリュームに一時ファイルを作成し、ファイルシステム経由でのみ書き込み、生ディスクには直接書き込みません。テスト完了後、一時ファイルは自動的に削除されます。テストは実際の書き込みを行い、容量を消費してデバイスが発熱します。ボリュームに十分な空き容量があることを確認してください。", L"속도 테스트는 선택한 볼륨에 임시 파일을 만들고 파일 시스템을 통해서만 쓰며 원시 디스크에 직접 쓰지 않습니다. 테스트가 끝나면 임시 파일이 자동으로 삭제됩니다. 테스트는 실제 쓰기를 수행하고 공간을 차지하며 장치가 발열되므로 볼륨에 충분한 여유 공간이 있는지 확인하세요.", L"Тест скорости создаёт временный файл на выбранном томе, записывает только через файловую систему и никогда напрямую на необработанный диск; по завершении теста файл удаляется автоматически. Тест выполняет реальную запись, занимает место и нагревает устройство; убедитесь, что на томе достаточно свободного места." },
{ L"未找到可写卷（请确认设备已挂载并具有写权限）", L"No writable volume found (make sure the device is mounted and writable)", L"Aucun volume inscriptible trouvé (vérifiez que l'appareil est monté et accessible en écriture)", L"Kein beschreibbares Volume gefunden (stellen Sie sicher, dass das Gerät eingebunden und beschreibbar ist)", L"No se encontró ningún volumen escribible (asegúrese de que el dispositivo esté montado y tenga permiso de escritura)", L"書き込み可能なボリュームが見つかりません（デバイスがマウントされ、書き込み権限があることを確認してください）", L"쓰기 가능한 볼륨을 찾을 수 없습니다(장치가 탑재되어 있고 쓰기 권한이 있는지 확인하세요).", L"Записываемый том не найден (убедитесь, что устройство подключено и доступно для записи)." },
{ L"USB 存储设备", L"USB storage device", L"Périphérique de stockage USB", L"USB-Speichergerät", L"Dispositivo de almacenamiento USB", L"USB ストレージデバイス", L"USB 저장 장치", L"USB-накопитель" },
{ L"桥接: %s", L"Bridge: %s", L"Pont : %s", L"Bridge: %s", L"Puente: %s", L"ブリッジ: %s", L"브리지: %s", L"Мост: %s" },
{ L"，可用 %s", L", %s free", L", %s libres", L", %s frei", L", %s libres", L"、空き %s", L", 여유 공간 %s", L", свободно %s" },
{ L"（只读）", L"(read-only)", L"(lecture seule)", L"(schreibgeschützt)", L"(solo lectura)", L"（読み取り専用）", L"(읽기 전용)", L"(только чтение)" },
{ L"请选择卷和文件大小，然后点击“开始测速”。", L"Select a volume and file size, then click \"Start speed test\".", L"Sélectionnez un volume et une taille de fichier, puis cliquez sur « Démarrer le test ».", L"Wählen Sie ein Volume und eine Dateigröße und klicken Sie dann auf „Test starten“.", L"Seleccione un volumen y un tamaño de archivo y haga clic en «Iniciar prueba».", L"ボリュームとファイルサイズを選択し、「速度テスト開始」をクリックしてください。", L"볼륨과 파일 크기를 선택한 후 \"속도 테스트 시작\"을 클릭하세요.", L"Выберите том и размер файла, затем нажмите «Начать тест»." },
{ L"仅 USB 存储设备支持测速", L"Speed test is only available for USB storage devices", L"Le test de vitesse n'est disponible que pour les périphériques de stockage USB", L"Der Geschwindigkeitstest ist nur für USB-Speichergeräte verfügbar", L"La prueba de velocidad solo está disponible para dispositivos de almacenamiento USB", L"速度テストは USB ストレージデバイスでのみ利用できます", L"속도 테스트는 USB 저장 장치에서만 지원됩니다", L"Тест скорости доступен только для USB-накопителей" },
{ L"仅 USB 存储设备支持测速。", L"Speed test is only available for USB storage devices.", L"Le test de vitesse n'est disponible que pour les périphériques de stockage USB.", L"Der Geschwindigkeitstest ist nur für USB-Speichergeräte verfügbar.", L"La prueba de velocidad solo está disponible para dispositivos de almacenamiento USB.", L"速度テストは USB ストレージデバイスでのみ利用できます。", L"속도 테스트는 USB 저장 장치에서만 지원됩니다.", L"Тест скорости доступен только для USB-накопителей." },
{ L"请选择一个 USB 存储设备节点。", L"Select a USB storage device node.", L"Sélectionnez un nœud de périphérique de stockage USB.", L"Wählen Sie einen USB-Speichergeräteknoten.", L"Seleccione un nodo de dispositivo de almacenamiento USB.", L"USB ストレージデバイスのノードを選択してください。", L"USB 저장 장치 노드를 선택하세요.", L"Выберите узел USB-накопителя." },

// ---------------------------------------------------------------------------
// Benchmark page: confirmation dialogs / runtime messages
// ---------------------------------------------------------------------------
{ L"所选卷没有可用的写路径或为只读。", L"The selected volume has no writable path or is read-only.", L"Le volume sélectionné n'a pas de chemin inscriptible ou est en lecture seule.", L"Das ausgewählte Volume hat keinen beschreibbaren Pfad oder ist schreibgeschützt.", L"El volumen seleccionado no tiene una ruta de escritura o es de solo lectura.", L"選択したボリュームには書き込み可能なパスがないか、読み取り専用です。", L"선택한 볼륨에 쓰기 경로가 없거나 읽기 전용입니다.", L"У выбранного тома нет пути для записи или он доступен только для чтения." },
{ L"USBInfo 测速", L"USBInfo speed test", L"USBInfo – Test de vitesse", L"USBInfo – Geschwindigkeitstest", L"USBInfo – Prueba de velocidad", L"USBInfo 速度テスト", L"USBInfo 속도 테스트", L"USBInfo — тест скорости" },
{ L"将在以下卷创建临时测速文件：\n\n%s\n\n大文件：%llu MB × 4 项（1 MiB 块：顺序写/读、随机写/读）\n小数据：%llu MB × 4 项（4 KiB 块：顺序写/读、随机写/读）\n\n测试只写文件系统，不会写裸盘；完成后自动删除。\n测试会产生实际写入并占用空间，是否继续？", L"Create temporary speed-test files on the following volume:\n\n%s\n\nLarge file: %llu MB × 4 tests (1 MiB blocks: sequential write/read, random write/read)\nSmall data: %llu MB × 4 tests (4 KiB blocks: sequential write/read, random write/read)\n\nThe test writes only through the file system, never to the raw disk; files are deleted automatically afterwards.\nThe test performs real writes and uses space. Continue?", L"Créer des fichiers temporaires de test sur le volume suivant :\n\n%s\n\nGros fichier : %llu Mo × 4 tests (blocs de 1 Mio : écriture/lecture séquentielle, écriture/lecture aléatoire)\nPetites données : %llu Mo × 4 tests (blocs de 4 Kio : écriture/lecture séquentielle, écriture/lecture aléatoire)\n\nLe test écrit uniquement via le système de fichiers, jamais sur le disque brut ; les fichiers sont supprimés automatiquement ensuite.\nLe test effectue de vraies écritures et occupe de l'espace. Continuer ?", L"Temporäre Testdateien auf dem folgenden Volume erstellen:\n\n%s\n\nGroße Datei: %llu MB × 4 Tests (1-MiB-Blöcke: sequenzielles Schreiben/Lesen, zufälliges Schreiben/Lesen)\nKleine Daten: %llu MB × 4 Tests (4-KiB-Blöcke: sequenzielles Schreiben/Lesen, zufälliges Schreiben/Lesen)\n\nDer Test schreibt nur über das Dateisystem, nie direkt auf den Rohdatenträger; die Dateien werden danach automatisch gelöscht.\nDer Test verursacht echte Schreibvorgänge und belegt Speicherplatz. Fortfahren?", L"Se crearán archivos temporales de prueba en el siguiente volumen:\n\n%s\n\nArchivo grande: %llu MB × 4 pruebas (bloques de 1 MiB: escritura/lectura secuencial, escritura/lectura aleatoria)\nDatos pequeños: %llu MB × 4 pruebas (bloques de 4 KiB: escritura/lectura secuencial, escritura/lectura aleatoria)\n\nLa prueba escribe solo a través del sistema de archivos, nunca en el disco sin formato; los archivos se eliminan automáticamente después.\nLa prueba realiza escrituras reales y ocupa espacio. ¿Continuar?", L"次のボリュームに一時テストファイルを作成します：\n\n%s\n\n大ファイル：%llu MB × 4 項目（1 MiB ブロック：シーケンシャル書き込み/読み取り、ランダム書き込み/読み取り）\n小データ：%llu MB × 4 項目（4 KiB ブロック：シーケンシャル書き込み/読み取り、ランダム書き込み/読み取り）\n\nテストはファイルシステム経由でのみ書き込み、生ディスクには書き込みません。終了後、自動的に削除されます。\nテストは実際の書き込みを行い、容量を消費します。続行しますか？", L"다음 볼륨에 임시 테스트 파일을 만듭니다:\n\n%s\n\n대용량 파일: %llu MB × 4개 항목(1 MiB 블록: 순차 쓰기/읽기, 임의 쓰기/읽기)\n소량 데이터: %llu MB × 4개 항목(4 KiB 블록: 순차 쓰기/읽기, 임의 쓰기/읽기)\n\n테스트는 파일 시스템을 통해서만 쓰고 원시 디스크에는 쓰지 않습니다. 완료 후 자동으로 삭제됩니다.\n테스트는 실제 쓰기를 수행하고 공간을 차지합니다. 계속하시겠습니까?", L"На следующем томе будут созданы временные тестовые файлы:\n\n%s\n\nБольшой файл: %llu МБ × 4 теста (блоки 1 МиБ: последовательная запись/чтение, случайная запись/чтение)\nМалые данные: %llu МБ × 4 теста (блоки 4 КиБ: последовательная запись/чтение, случайная запись/чтение)\n\nТест пишет только через файловую систему, никогда напрямую на необработанный диск; по завершении файлы удаляются автоматически.\nТест выполняет реальную запись и занимает место. Продолжить?" },
{ L"USBInfo 测速确认", L"USBInfo speed test confirmation", L"USBInfo – Confirmation du test de vitesse", L"USBInfo – Bestätigung des Geschwindigkeitstests", L"USBInfo – Confirmación de la prueba de velocidad", L"USBInfo 速度テストの確認", L"USBInfo 속도 테스트 확인", L"USBInfo — подтверждение теста скорости" },
{ L"准备测试文件…", L"Preparing test files…", L"Préparation des fichiers de test…", L"Testdateien werden vorbereitet…", L"Preparando archivos de prueba…", L"テストファイルを準備しています…", L"테스트 파일 준비 중…", L"Подготовка тестовых файлов…" },
{ L"正在创建临时测试文件…", L"Creating temporary test files…", L"Création des fichiers de test temporaires…", L"Temporäre Testdateien werden erstellt…", L"Creando archivos de prueba temporales…", L"一時テストファイルを作成しています…", L"임시 테스트 파일 생성 중…", L"Создание временных тестовых файлов…" },
{ L"测速线程异常：", L"Speed test thread error: ", L"Erreur du thread de test : ", L"Fehler im Test-Thread: ", L"Error del subproceso de prueba: ", L"速度テストスレッドのエラー: ", L"속도 테스트 스레드 오류: ", L"Ошибка потока теста скорости: " },
{ L"正在取消测试并清理临时文件…", L"Cancelling the test and cleaning up temporary files…", L"Annulation du test et nettoyage des fichiers temporaires…", L"Test wird abgebrochen und temporäre Dateien werden bereinigt…", L"Cancelando la prueba y limpiando los archivos temporales…", L"テストをキャンセルし、一時ファイルをクリーンアップしています…", L"테스트를 취소하고 임시 파일을 정리하는 중…", L"Отмена теста и очистка временных файлов…" },
{ L"正在取消测试…", L"Cancelling the test…", L"Annulation du test…", L"Test wird abgebrochen…", L"Cancelando la prueba…", L"テストをキャンセルしています…", L"테스트 취소 중…", L"Отмена теста…" },
{ L"测速完成。临时文件已删除。", L"Speed test complete. Temporary files deleted.", L"Test de vitesse terminé. Fichiers temporaires supprimés.", L"Geschwindigkeitstest abgeschlossen. Temporäre Dateien gelöscht.", L"Prueba de velocidad completada. Archivos temporales eliminados.", L"速度テストが完了しました。一時ファイルは削除されました。", L"속도 테스트가 완료되었습니다. 임시 파일이 삭제되었습니다.", L"Тест скорости завершён. Временные файлы удалены." },
{ L"测速完成", L"Speed test complete", L"Test de vitesse terminé", L"Geschwindigkeitstest abgeschlossen", L"Prueba de velocidad completada", L"速度テスト完了", L"속도 테스트 완료", L"Тест скорости завершён" },
{ L"测速未完成。", L"Speed test did not finish.", L"Le test de vitesse ne s'est pas terminé.", L"Der Geschwindigkeitstest wurde nicht abgeschlossen.", L"La prueba de velocidad no finalizó.", L"速度テストが完了しませんでした。", L"속도 테스트가 완료되지 않았습니다.", L"Тест скорости не завершён." },
{ L"测速未完成。 %s", L"Speed test did not finish. %s", L"Le test de vitesse ne s'est pas terminé. %s", L"Der Geschwindigkeitstest wurde nicht abgeschlossen. %s", L"La prueba de velocidad no finalizó. %s", L"速度テストが完了しませんでした。%s", L"속도 테스트가 완료되지 않았습니다. %s", L"Тест скорости не завершён. %s" },
{ L"测试已取消", L"Test cancelled", L"Test annulé", L"Test abgebrochen", L"Prueba cancelada", L"テストはキャンセルされました", L"테스트 취소됨", L"Тест отменён" },
{ L"测速未完成", L"Speed test incomplete", L"Test de vitesse incomplet", L"Geschwindigkeitstest unvollständig", L"Prueba de velocidad incompleta", L"速度テスト未完了", L"속도 테스트 미완료", L"Тест скорости не завершён" },
{ L"提示", L"Tip", L"Conseil", L"Hinweis", L"Aviso", L"ヒント", L"팁", L"Совет" },

// ---------------------------------------------------------------------------
// Benchmark page: result table + context menu
// ---------------------------------------------------------------------------
{ L"测试", L"Test", L"Test", L"Test", L"Prueba", L"テスト", L"테스트", L"Тест" },
{ L"数据量", L"Data", L"Données", L"Datenmenge", L"Datos", L"データ量", L"데이터량", L"Объём данных" },
{ L"耗时", L"Time", L"Durée", L"Dauer", L"Tiempo", L"所要時間", L"소요 시간", L"Время" },
{ L"速度", L"Speed", L"Débit", L"Geschwindigkeit", L"Velocidad", L"速度", L"속도", L"Скорость" },
{ L"延迟", L"Latency", L"Latence", L"Latenz", L"Latencia", L"レイテンシ", L"지연 시간", L"Задержка" },
{ L"完成", L"Done", L"Terminé", L"Fertig", L"Completado", L"完了", L"완료", L"Готово" },
{ L"失败", L"Failed", L"Échec", L"Fehlgeschlagen", L"Fallido", L"失敗", L"실패", L"Ошибка" },
{ L"复制当前行", L"Copy current row", L"Copier la ligne actuelle", L"Aktuelle Zeile kopieren", L"Copiar fila actual", L"現在の行をコピー", L"현재 행 복사", L"Копировать текущую строку" },
{ L"复制全部结果", L"Copy all results", L"Copier tous les résultats", L"Alle Ergebnisse kopieren", L"Copiar todos los resultados", L"すべての結果をコピー", L"모든 결과 복사", L"Копировать все результаты" },

// ---------------------------------------------------------------------------
// Benchmark page: measurement value formats (units are localized where useful)
// ---------------------------------------------------------------------------
{ L"%.3f s", L"%.3f s", L"%.3f s", L"%.3f s", L"%.3f s", L"%.3f 秒", L"%.3f초", L"%.3f с" },
{ L"%.3f ms", L"%.3f ms", L"%.3f ms", L"%.3f ms", L"%.3f ms", L"%.3f ms", L"%.3f ms", L"%.3f мс" },
{ L"%.2f MB/s", L"%.2f MB/s", L"%.2f MB/s", L"%.2f MB/s", L"%.2f MB/s", L"%.2f MB/s", L"%.2f MB/s", L"%.2f MB/s" },

// ---------------------------------------------------------------------------
// Throughput chart
// ---------------------------------------------------------------------------
{ L"实时吞吐 (MB/s)", L"Real-time throughput (MB/s)", L"Débit en temps réel (MB/s)", L"Echtzeitdurchsatz (MB/s)", L"Rendimiento en tiempo real (MB/s)", L"リアルタイムスループット (MB/s)", L"실시간 처리량 (MB/s)", L"Пропускная способность в реальном времени (MB/s)" },
{ L"当前 %.2f MB/s", L"Current %.2f MB/s", L"Actuel %.2f MB/s", L"Aktuell %.2f MB/s", L"Actual %.2f MB/s", L"現在 %.2f MB/s", L"현재 %.2f MB/s", L"Текущая %.2f MB/s" },
{ L"平均 %.2f MB/s", L"Average %.2f MB/s", L"Moyenne %.2f MB/s", L"Durchschnitt %.2f MB/s", L"Media %.2f MB/s", L"平均 %.2f MB/s", L"평균 %.2f MB/s", L"Средняя %.2f MB/s" },
{ L"当前 -- MB/s", L"Current -- MB/s", L"Actuel -- MB/s", L"Aktuell -- MB/s", L"Actual -- MB/s", L"現在 -- MB/s", L"현재 -- MB/s", L"Текущая -- MB/s" },
{ L"平均 -- MB/s", L"Average -- MB/s", L"Moyenne -- MB/s", L"Durchschnitt -- MB/s", L"Media -- MB/s", L"平均 -- MB/s", L"평균 -- MB/s", L"Средняя -- MB/s" },
{ L"等待测速…", L"Waiting for the speed test…", L"En attente du test de vitesse…", L"Warten auf Geschwindigkeitstest…", L"En espera de la prueba de velocidad…", L"速度テストを待機しています…", L"속도 테스트 대기 중…", L"Ожидание теста скорости…" },
{ L"每项超时:", L"Timeout:", L"Délai :", L"Timeout:", L"Tiempo:", L"タイムアウト:", L"제한 시간:", L"Тайм-аут:" },
{ L"%u 秒", L"%u sec", L"%u s", L"%u s", L"%u s", L"%u 秒", L"%u초", L"%u с" },
{ L"无限制", L"Unlimited", L"Illimité", L"Unbegrenzt", L"Ilimitado", L"無制限", L"무제한", L"Без ограничения" },
