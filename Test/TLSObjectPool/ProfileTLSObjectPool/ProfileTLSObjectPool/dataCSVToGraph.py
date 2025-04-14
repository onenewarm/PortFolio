import csv
import matplotlib.pyplot as plt
import os
from adjustText import adjust_text

# 현재 스크립트 파일이 위치한 디렉터리 경로
script_dir = os.path.dirname(os.path.abspath(__file__))

# CSV 파일 경로
alloc_profile_file = os.path.join(script_dir, 'alloc.csv')
free_profile_file = os.path.join(script_dir, 'free.csv')
csv_files = [alloc_profile_file, free_profile_file]

titles = ['My Alloc vs New', 'My Free vs Delete']
titleIdx = 0

for csv_file in csv_files:
    data_rows = []
    
    # CSV 파일 읽기
    with open(csv_file, newline='', encoding='utf-8') as file:
        reader = csv.reader(file, delimiter=',')
        for row in reader:
            if row:  # 빈 행 제외
                data_rows.append(row)

    if not data_rows:
        continue

    # 첫 번째 행(헤더): x축 데이터
    header = data_rows[0]
    try:
        x_values = [str(val) for val in header[1:]]
    except ValueError:
        print(f"{csv_file}: x축 데이터 변환에 문제가 있습니다.")
        continue

    # 새로운 그래프 생성 및 텍스트 객체 리스트 초기화
    plt.figure(figsize=(12, 8))
    texts1 = []
    texts2 = []    

    rowIdx = 0
    # 두 번째 행부터 실제 데이터 처리
    for row in data_rows[1:]:
        row_label = row[0]
        try:
            # 마이크로초 -> 밀리초 보정 
            y_values = [int(val) / 10000 for val in row[1:]]
        except ValueError:
            print(f"{csv_file}의 {row_label} 데이터 변환에 문제가 있습니다.")
            continue

        # 라인 플롯 생성
        line = plt.plot(x_values, y_values, marker='o', label=row_label)

        # 각 데이터 포인트 위에 값 표시
        for idx, y_val in enumerate(y_values):
            text_obj = plt.text(
                x_values[idx],
                y_val,
                f"{int(y_val)}ms",
                ha='center',
                va='bottom',
                fontsize=12,
                rotation=0,
                weight='semibold',
                color=line[0].get_color()
            )


    plt.xlabel("Thread Count")
    plt.ylabel("Time (ms)")
    plt.title(titles[titleIdx], fontsize=20, fontweight='bold', color='blue')
    titleIdx += 1
    plt.legend(title="Row")
    plt.grid(True)

    # 결과를 PNG로 저장
    png_file = os.path.join(
        script_dir,
        os.path.splitext(os.path.basename(csv_file))[0] + '_graph_ms_log.png'
    )
    plt.savefig(png_file, dpi=300, bbox_inches='tight')

    # 그래프 표시 및 현재 Figure 해제
    plt.show()
    plt.close()
