#!/usr/bin/env python3

# python3 perf_compare.py -d /mnt/d/Code-Suite/UTS/Test-Suite/diff_tests/perf_history/ -eoe -osc -T 10

import copy
import json
import os
import sys

import click
from colorama import init
from tabulate import tabulate
from termcolor import colored

TABLEFMT_CHOICES = [
    "plain",
    "simple",
    "github",
    "grid",
    "fancy_grid",
    "pipe",
    "orgtbl",
    "jira",
    "presto",
    "psql",
    "rst",
    "mediawiki",
    "moinmoin",
    "youtrack",
    "html",
    "latex",
    "latex_raw",
    "latex_booktabs",
    "textile",
]


def get_recent_files(dir):
    files = sorted(os.listdir(dir))
    return f"{dir}{files[-1]}", f"{dir}{files[-2]}"


def validate_metric(string):
    words = string.split()
    if len(words) != 2:
        return False
    if words[0].isalnum() and words[1].replace(".", "", 1).isdigit():
        return True
    return False


def load_metrics(filename):
    content = ""
    metrics = {}
    curr_program = ""
    curr_section = ""
    with open(filename) as f:
        content = f.readlines()
    for index, line in enumerate(content):
        line = line.strip()

        # check if line is a section heading, and keep track of it
        if line.startswith("***") and not line == len(line) * line[0]:
            try:
                # check if heading is being used again for the same program
                if metrics.get(curr_program).get(line):
                    last_char = line[-1]

                    # if heading is being used again, append nth use to the string
                    if last_char.isdigit():
                        last_char = int(last_char) + 1
                    else:
                        last_char = "2"
                    curr_section = line + last_char
                else:
                    curr_section = line

            # if heading is new for the current program,
            # then leave string alone
            except (AttributeError, KeyError):
                curr_section = line

            # since line is a heading, skip to the next line
            continue

        # grab program name and keep track of it
        if line.startswith("###") and "program : " in line:
            curr_program = line.split()[3].replace(")", "").replace("#", "")
            metrics.setdefault(curr_program, {})
            metrics[curr_program][curr_section] = {}
            continue

        if not validate_metric(line):
            continue

        words = line.split()
        key = words[0]
        value = words[1]
        metrics[curr_program][curr_section][key] = {
            "line": index + 1,
            "currentValue": value,
        }
    return metrics


def is_over_threshold(value, threshold):
    return abs(value) >= threshold


def print_table(
    metrics,
    print_line=False,
    use_color=True,
    tablefmt="plain",
    threshold=10,
    only_show_changed=False,
):
    headers = [
        "Metric",
        "Previous Value",
        "Current Value",
        "Difference",
        "% Difference",
    ]
    metrics_over_threshold = []
    if print_line:
        headers.append("Line")
    for program, sections in metrics.items():
        print(program)
        for section, attributes in sections.items():
            print(section)
            table = []
            perc_arr = []
            for key, value in attributes.items():
                percentage = value.get("percentage")
                if is_over_threshold(percentage, threshold):
                    metrics_over_threshold.append(
                        f"{program}/{section}/{key} = {value['previousValue']} -> {value['currentValue']} ({percentage}%)"
                    )
                if not use_color:
                    pass
                elif percentage is None:
                    pass
                elif percentage > 0:
                    percentage = colored(percentage, "white", "on_green")
                elif percentage < 0:
                    percentage = colored(percentage, "white", "on_red")
                elif percentage == 0:
                    if only_show_changed:
                        continue
                row = [
                    key + " (NEW!)"
                    if value.get("previousValue") is None
                    and value.get("currentValue") is not None
                    else key,
                    value.get("previousValue"),
                    value.get("currentValue"),
                    value.get("difference"),
                    percentage,
                ]
                if print_line:
                    row.append(value.get("line"))
                table.append(row)
            print(tabulate(table, headers, tablefmt=tablefmt), end="\n\n\n")    
    return metrics_over_threshold


def load_number(str):
    num = 0
    try:
        num = int(str)
    except ValueError:
        try:
            num = float(str)
        except:
            return None
    return num


def compare_metrics(older, newer):
    metrics = copy.deepcopy(newer)
    for program, sections in older.items():
        if not metrics.get(program):
            metrics[program + " (REMOVED)"] = {}
            continue
        for section, attributes in sections.items():
            if not metrics.get(program).get(section):
                metrics[program][section + " (REMOVED)"] = {}
                continue
            for attribute, stat in attributes.items():
                if not metrics.get(program).get(section).get(attribute):
                    metrics[program][section][attribute + " (REMOVED)"] = {}
                    continue
                previous_value = load_number(
                    older[program][section][attribute]["currentValue"]
                )
                try:
                    current_value = load_number(
                        newer[program][section][attribute]["currentValue"]
                    )
                except:
                    continue
                metrics[program][section][attribute]["difference"] = (
                    current_value - previous_value
                )
                metrics[program][section][attribute]["previousValue"] = previous_value
                if current_value != 0:
                    metrics[program][section][attribute]["percentage"] = (
                        (current_value - previous_value) / current_value * 100
                    )
                elif current_value == 0 and previous_value == 0:
                    metrics[program][section][attribute]["percentage"] = 0
                else:
                    metrics[program][section][attribute]["percentage"] = None
    return metrics


@click.command()
@click.option(
    "-d",
    "--target-directory",
    type=click.Path(exists=True, dir_okay=True, file_okay=False),
    required=True,
    help="directory with the two most recent perf files.",
)
@click.option(
    "-p/-P",
    "--print-line/--no-print-line",
    default=False,
    help="print the line number where the metrics is located in the newest file.",
)
@click.option(
    "-c/-C",
    "--use-color/--no-use-color",
    default=True,
    help="enable/disable terminal colors",
)
@click.option(
    "-f1",
    "--file1",
    type=click.Path(exists=True, dir_okay=False, file_okay=True),
    help="the older file.",
)
@click.option(
    "-f2",
    "--file2",
    type=click.Path(exists=True, dir_okay=False, file_okay=True),
    help="the newer file.",
)
@click.option(
    "-t",
    "--tablefmt",
    help="choose table format.",
#     type=click.Choice(TABLEFMT_CHOICES, case_sensitive=False),
    type=click.Choice(TABLEFMT_CHOICES),
    default="plain",
    show_default=True,
)
@click.option(
    "-T",
    "--threshold-percentage",
    help="show metrics with a change greater than or equal to the threshold percentage.",
    type=int,
    default=10,
    show_default=True,
)
@click.option(
    "-eoe/-Eoe",
    "--exit-on-error/--no-exit-on-error",
    help="exit on error if any metrics are greater than or equal to --threshold-percentage.",
    default=False,
    show_default=True,
)
@click.option(
    "-osc/-Osc",
    "--only-show-changed/--no-only-show-changed",
    help="only show values that changed.",
    default=False,
    show_default=True,
)
def main(
    target_directory,
    print_line,
    use_color,
    file1,
    file2,
    tablefmt,
    threshold_percentage,
    exit_on_error,
    only_show_changed,
):
    init()

    newest_metrics_text, previous_metrics_text = (
        [file2, file1] if file1 and file2 else get_recent_files(dir=target_directory)
    )

    newest_metrics = load_metrics(newest_metrics_text)

    previous_metrics = load_metrics(previous_metrics_text)

    metrics_over_threshold = print_table(
        compare_metrics(previous_metrics, newest_metrics),
        print_line=print_line,
        use_color=use_color,
        tablefmt=tablefmt,
        threshold=threshold_percentage,
        only_show_changed=only_show_changed,
    )

    if metrics_over_threshold:
        print(f"Changes greater than or equal to the threshold ({threshold_percentage}%):")
        print(json.dumps(metrics_over_threshold, indent=2))
        if exit_on_error:
            sys.exit(1)


if __name__ == "__main__":
    main()
