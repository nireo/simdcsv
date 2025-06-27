import csv
import random
import string
from datetime import datetime, timedelta
import argparse
import sys


class CSVGenerator:
    def __init__(self, num_rows=10000, num_columns=10, filename="large_data.csv"):
        self.num_rows = num_rows
        self.num_columns = num_columns
        self.filename = filename

        self.first_names = [
            "John",
            "Jane",
            "Michael",
            "Sarah",
            "David",
            "Emily",
            "Chris",
            "Lisa",
            "Robert",
            "Anna",
        ]
        self.last_names = [
            "Smith",
            "Johnson",
            "Williams",
            "Brown",
            "Jones",
            "Garcia",
            "Miller",
            "Davis",
            "Rodriguez",
            "Martinez",
        ]
        self.cities = [
            "New York",
            "Los Angeles",
            "Chicago",
            "Houston",
            "Phoenix",
            "Philadelphia",
            "San Antonio",
            "San Diego",
            "Dallas",
            "San Jose",
        ]
        self.departments = [
            "Engineering",
            "Marketing",
            "Sales",
            "HR",
            "Finance",
            "Operations",
            "IT",
            "Legal",
            "Research",
            "Support",
        ]
        self.products = [
            "Widget A",
            "Gadget B",
            "Tool C",
            "Device D",
            "Component E",
            "Module F",
            "System G",
            "Unit H",
            "Part I",
            "Item J",
        ]

    def generate_random_string(self, length=10):
        return "".join(random.choices(string.ascii_letters + string.digits, k=length))

    def generate_random_date(self, start_year=2020, end_year=2024):
        start_date = datetime(start_year, 1, 1)
        end_date = datetime(end_year, 12, 31)
        time_between = end_date - start_date
        days_between = time_between.days
        random_days = random.randrange(days_between)
        return (start_date + timedelta(days=random_days)).strftime("%Y-%m-%d")

    def generate_column_data(self, column_index, row_index):
        column_types = [
            lambda: f"ID_{row_index:06d}",
            lambda: random.choice(self.first_names),
            lambda: random.choice(self.last_names),
            lambda: f"{random.choice(self.first_names).lower()}.{random.choice(self.last_names).lower()}@company.com",
            lambda: random.choice(self.cities),
            lambda: random.choice(self.departments),
            lambda: round(random.uniform(30000, 120000), 2),
            lambda: random.randint(18, 65),
            lambda: self.generate_random_date(),
            lambda: random.choice(self.products),
            lambda: round(random.uniform(10.50, 999.99), 2),
            lambda: random.randint(1, 1000),
            lambda: random.choice([True, False]),
            lambda: round(random.gauss(100, 15), 2),
            lambda: self.generate_random_string(random.randint(5, 20)),
        ]

        generator = column_types[column_index % len(column_types)]
        return generator()

    def generate_headers(self):
        base_headers = [
            "ID",
            "FirstName",
            "LastName",
            "Email",
            "City",
            "Department",
            "Salary",
            "Age",
            "Date",
            "Product",
            "Price",
            "Quantity",
            "Active",
            "Score",
            "Notes",
        ]

        headers = []
        for i in range(self.num_columns):
            if i < len(base_headers):
                headers.append(base_headers[i])
            else:
                headers.append(f"Column_{i+1}")

        return headers

    def generate_csv(self):
        print(
            f"Generating CSV with {self.num_rows:,} rows and {self.num_columns} columns..."
        )
        print(f"Output file: {self.filename}")

        headers = self.generate_headers()

        try:
            with open(self.filename, "w", newline="", encoding="utf-8") as csvfile:
                writer = csv.writer(csvfile)

                writer.writerow(headers)

                for row_index in range(self.num_rows):
                    row_data = []
                    for col_index in range(self.num_columns):
                        row_data.append(self.generate_column_data(col_index, row_index))

                    writer.writerow(row_data)

                    if (row_index + 1) % 10000 == 0:
                        print(f"Generated {row_index + 1:,} rows...")

            print(
                f"Successfully generated {self.filename} with {self.num_rows:,} rows!"
            )

            import os

            file_size = os.path.getsize(self.filename)
            if file_size > 1024 * 1024:
                print(f"File size: {file_size / (1024 * 1024):.2f} MB")
            else:
                print(f"File size: {file_size / 1024:.2f} KB")

        except Exception as e:
            print(f"Error generating CSV: {e}")
            sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description="Generate a large CSV file with sample data"
    )
    parser.add_argument(
        "--rows",
        "-r",
        type=int,
        default=10000,
        help="Number of rows to generate (default: 10000)",
    )
    parser.add_argument(
        "--columns",
        "-c",
        type=int,
        default=10,
        help="Number of columns to generate (default: 10)",
    )
    parser.add_argument(
        "--filename",
        "-f",
        type=str,
        default="large_data.csv",
        help="Output filename (default: large_data.csv)",
    )

    args = parser.parse_args()

    if args.rows <= 0:
        print("Error: Number of rows must be positive")
        sys.exit(1)

    if args.columns <= 0:
        print("Error: Number of columns must be positive")
        sys.exit(1)

    generator = CSVGenerator(args.rows, args.columns, args.filename)
    generator.generate_csv()


if __name__ == "__main__":
    main()
