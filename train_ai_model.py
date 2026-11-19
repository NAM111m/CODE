import pandas as pd
import numpy as np
import psycopg2
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from sklearn.ensemble import RandomForestClassifier
import pickle

# ==== CẤU HÌNH KẾT NỐI DATABASE ====
DB_CONFIG = {
    "host": "localhost",
    "database": "IoT_DoAnTotNghiep",
    "user": "postgres",
    "password": "111212"
}

# ==== LẤY DỮ LIỆU TỪ BẢNG machine_data ====
conn = psycopg2.connect(**DB_CONFIG)
query = """
SELECT temperature, current, accel_x, accel_y, accel_z, timestamp
FROM machine_data
WHERE temperature IS NOT NULL
  AND current IS NOT NULL
  AND accel_x IS NOT NULL
  AND accel_y IS NOT NULL
  AND accel_z IS NOT NULL
ORDER BY timestamp;
"""
df = pd.read_sql(query, conn)
conn.close()

print("✅ Đã đọc dữ liệu:", df.shape)

# ==== TÍNH VIBRATION RMS THEO CÔNG THỨC CHUẨN ====
# Công thức: A_RMS = sqrt( (1/N) * sum_{i=1}^{N} |a_i|^2 )
# Trong đó: |a_i| = sqrt(x² + y² + z²) là magnitude của mẫu thứ i
# N = số mẫu trong cửa sổ (window size)

# Bước 1: Tính magnitude cho mỗi mẫu: |a_i| = sqrt(x² + y² + z²)
df["magnitude"] = np.sqrt(df["accel_x"]**2 + df["accel_y"]**2 + df["accel_z"]**2)

# Bước 2: Tính RMS qua N mẫu: A_RMS = sqrt( mean(|a_i|²) )
# Sử dụng rolling window với N=100 mẫu (giống với mqtt_subscriber.py)
RMS_WINDOW_SIZE = 100
df["vibration_rms"] = df["magnitude"].rolling(
    window=RMS_WINDOW_SIZE, 
    min_periods=RMS_WINDOW_SIZE
).apply(
    lambda x: np.sqrt(np.mean(x**2)) if len(x) == RMS_WINDOW_SIZE else np.nan
)

# Loại bỏ các dòng có vibration_rms = NaN (do không đủ mẫu trong cửa sổ)
df = df.dropna(subset=["vibration_rms"])

print(f"✅ Đã tính vibration_rms theo công thức RMS chuẩn (window={RMS_WINDOW_SIZE} mẫu)")
print(f"   Số dòng sau khi tính RMS: {df.shape[0]}")

# ==== SINH DỮ LIỆU WARNING VÀ FAULT NHÂN TẠO ====
# Tạo dữ liệu synthetic để đảm bảo model có thể học được cả 3 classes
np.random.seed(42)  # Đảm bảo kết quả có thể tái lập

# Số lượng mẫu synthetic cho mỗi class (cân bằng với dữ liệu NORMAL)
num_normal = df.shape[0]
num_warning = min(1000, num_normal // 2)  # Tạo 50% số lượng NORMAL hoặc tối đa 1000
num_fault = min(500, num_normal // 4)     # Tạo 25% số lượng NORMAL hoặc tối đa 500

print(f"\n🔧 Sinh dữ liệu nhân tạo:")
print(f"   - WARNING: {num_warning} mẫu")
print(f"   - FAULT: {num_fault} mẫu")

# ==== TẠO DỮ LIỆU WARNING ====
# WARNING: 50 < temp <= 70, 3.0 < vib_rms <= 5.0, 7.0 < current <= 10.0
warning_data = []

for _ in range(num_warning):
    # Chọn ngẫu nhiên một trong 3 điều kiện để vượt ngưỡng WARNING
    condition = np.random.choice(['temp', 'vib', 'current'])
    
    if condition == 'temp':
        temp = np.random.uniform(50.1, 70.0)
        vib_rms = np.random.uniform(2.0, 4.5)  # Có thể vượt hoặc không
        current = np.random.uniform(5.0, 8.0)
    elif condition == 'vib':
        temp = np.random.uniform(30.0, 50.0)
        vib_rms = np.random.uniform(3.1, 5.0)
        current = np.random.uniform(5.0, 8.0)
    else:  # current
        temp = np.random.uniform(30.0, 50.0)
        vib_rms = np.random.uniform(2.0, 4.5)
        current = np.random.uniform(7.1, 10.0)
    
    # Tính toán accel từ vibration_rms (giả sử magnitude ≈ vibration_rms cho đơn giản)
    # magnitude = sqrt(x² + y² + z²), giả sử z ≈ 1g (trọng lực), x, y nhỏ
    target_magnitude = vib_rms * 0.8  # Scale down một chút
    accel_z = np.random.uniform(0.9, 1.1)  # Gần 1g
    accel_x = np.random.uniform(-0.3, 0.3)
    accel_y = np.random.uniform(-0.3, 0.3)
    
    # Điều chỉnh để magnitude gần với target
    current_mag = np.sqrt(accel_x**2 + accel_y**2 + accel_z**2)
    if current_mag > 0:
        scale = target_magnitude / current_mag
        accel_x *= scale
        accel_y *= scale
        accel_z *= scale
    
    warning_data.append({
        'temperature': temp,
        'current': current,
        'accel_x': accel_x,
        'accel_y': accel_y,
        'accel_z': accel_z,
        'timestamp': None
    })

df_warning = pd.DataFrame(warning_data)

# Tính magnitude và vibration_rms cho dữ liệu WARNING
df_warning["magnitude"] = np.sqrt(df_warning["accel_x"]**2 + df_warning["accel_y"]**2 + df_warning["accel_z"]**2)
# Với dữ liệu synthetic, giả sử mỗi mẫu đại diện cho một cửa sổ đã tính RMS
df_warning["vibration_rms"] = df_warning["magnitude"]  # Đơn giản hóa cho synthetic data

# ==== TẠO DỮ LIỆU FAULT ====
# FAULT: temp > 70, vib_rms > 5.0, current > 10.0
fault_data = []

for _ in range(num_fault):
    # Chọn ngẫu nhiên một trong 3 điều kiện để vượt ngưỡng FAULT
    condition = np.random.choice(['temp', 'vib', 'current'])
    
    if condition == 'temp':
        temp = np.random.uniform(70.1, 90.0)
        vib_rms = np.random.uniform(3.0, 6.0)
        current = np.random.uniform(8.0, 12.0)
    elif condition == 'vib':
        temp = np.random.uniform(40.0, 70.0)
        vib_rms = np.random.uniform(5.1, 8.0)
        current = np.random.uniform(8.0, 12.0)
    else:  # current
        temp = np.random.uniform(40.0, 70.0)
        vib_rms = np.random.uniform(3.0, 6.0)
        current = np.random.uniform(10.1, 15.0)
    
    # Tính toán accel từ vibration_rms
    target_magnitude = vib_rms * 0.8
    accel_z = np.random.uniform(0.9, 1.2)
    accel_x = np.random.uniform(-0.5, 0.5)
    accel_y = np.random.uniform(-0.5, 0.5)
    
    current_mag = np.sqrt(accel_x**2 + accel_y**2 + accel_z**2)
    if current_mag > 0:
        scale = target_magnitude / current_mag
        accel_x *= scale
        accel_y *= scale
        accel_z *= scale
    
    fault_data.append({
        'temperature': temp,
        'current': current,
        'accel_x': accel_x,
        'accel_y': accel_y,
        'accel_z': accel_z,
        'timestamp': None
    })

df_fault = pd.DataFrame(fault_data)

# Tính magnitude và vibration_rms cho dữ liệu FAULT
df_fault["magnitude"] = np.sqrt(df_fault["accel_x"]**2 + df_fault["accel_y"]**2 + df_fault["accel_z"]**2)
df_fault["vibration_rms"] = df_fault["magnitude"]  # Đơn giản hóa cho synthetic data

# ==== KẾT HỢP DỮ LIỆU ====
# Kết hợp dữ liệu gốc với dữ liệu synthetic
df_combined = pd.concat([df, df_warning, df_fault], ignore_index=True)

print(f"✅ Đã kết hợp dữ liệu:")
print(f"   - Tổng số mẫu: {df_combined.shape[0]} (NORMAL: {num_normal}, WARNING: {num_warning}, FAULT: {num_fault})")

# ==== TẠO CỘT NHÃN failure với 3 classes ====
# 0 = NORMAL, 1 = WARNING, 2 = FAULT

def get_label(temp, vib_rms, current):
    """
    Phân loại trạng thái máy:
    - FAULT (2): Vượt ngưỡng nghiêm trọng
    - WARNING (1): Vượt ngưỡng cảnh báo
    - NORMAL (0): Bình thường
    """
    # Ngưỡng FAULT (nghiêm trọng)
    if temp > 70 or vib_rms > 5.0 or current > 10.0:
        return 2  # FAULT
    # Ngưỡng WARNING (cảnh báo)
    elif temp > 50 or vib_rms > 3.0 or current > 7.0:
        return 1  # WARNING
    # Bình thường
    else:
        return 0  # NORMAL

df_combined["failure"] = df_combined.apply(
    lambda row: get_label(row["temperature"], row["vibration_rms"], row["current"]), 
    axis=1
)

# Sử dụng df_combined thay vì df
df = df_combined

# ==== CHUẨN BỊ DỮ LIỆU ====
# Chỉ sử dụng các features gốc (không dùng vibration_rms vì nó được tính từ accel)
X = df[["temperature", "current", "accel_x", "accel_y", "accel_z"]]
y = df["failure"]

# Kiểm tra phân bố classes sau khi kết hợp
print("\n📊 Phân bố classes sau khi kết hợp dữ liệu:")
print(y.value_counts().sort_index())
print("0 = NORMAL, 1 = WARNING, 2 = FAULT")

# ==== CHUẨN HÓA ====
scaler = StandardScaler()
X_scaled = scaler.fit_transform(X)

# ==== HUẤN LUYỆN RANDOM FOREST ====
model = RandomForestClassifier(n_estimators=100, random_state=42)
model.fit(X_scaled, y)

# ==== LƯU MÔ HÌNH ====
with open("rf_model.pkl", "wb") as f:
    pickle.dump(model, f)

with open("scaler.pkl", "wb") as f:
    pickle.dump(scaler, f)

# In thông tin model
print(f"\n📈 Model Information:")
print(f"   - Classes: {model.classes_}")
print(f"   - Number of classes: {len(model.classes_)}")
print(f"   - Features: {X.shape[1]} features")

print("\n🎯 Hoàn tất huấn luyện — đã sinh ra rf_model.pkl và scaler.pkl")
print("   Model hỗ trợ 3 classes: NORMAL (0), WARNING (1), FAULT (2)")
